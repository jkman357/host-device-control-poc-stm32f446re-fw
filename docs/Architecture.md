# Firmware Architecture

## Runtime model

The firmware uses a non-blocking foreground dispatcher and interrupt-backed event sources.

```text
TIM6 ISR ───────────────┐
                        ├─> AppEvent pending state ─> main superloop ─> app_process_events()
USART2 RX/error ISR ────┘

main context:
  RX bytes → shared-Protocol parser → command dispatcher → IDLE/STREAMING state
  sample ticks → device_tick_us → float32 sine sample → TELEMETRY_SAMPLE → TX ring
```

Interrupt handlers do not parse protocol frames, execute commands, calculate sine values, or build telemetry.
They only move bytes, update bounded counters, and post coalesced event state.

## Responsibilities

### Core

- Reset and vector table
- C runtime data/BSS initialization
- Main event-dispatch loop
- Default 5,000-microsecond sample-timer startup

### Platform

- HSI 16 MHz clock selection
- PA5 LED setup
- PA2/PA3 USART alternate-function setup
- TIM6 1 MHz counter and configurable 1–65,535 microsecond period
- NVIC and critical-section helpers

### Transport

- USART2 at 115200 bps, 8-N-1
- 2048-byte RX and TX static byte rings
- Interrupt-driven receive and transmit
- Saturating overflow and UART error counters

### Protocol

- Authoritative 10-byte-overhead frame layout
- One-byte Message ID and two-byte payload length
- Maximum 1024-byte payload
- CRC-16/CCITT-FALSE
- Byte-wise parsing, resynchronization, and 250 ms partial-frame timeout
- Little-endian integer and IEEE-754 float32 serialization

### App

- Device state: IDLE or STREAMING
- Shared command validation and direct-response construction
- Configurable sample interval
- `uint32` device time and telemetry sample counter
- Independent unsolicited-message sequence
- 1 Hz float32 sine telemetry
- Sticky transport status reporting
- LED heartbeat

## Concurrency boundaries

- USART2 ISR is the sole RX-ring producer.
- Main context is the sole RX-ring consumer.
- Main context is the sole TX-ring producer.
- USART2 ISR is the sole TX-ring consumer.
- Event state is exchanged through short PRIMASK-protected snapshots.
- Protocol parsing, timer reconfiguration, state transitions, and frame construction run only in main context.
- No dynamic memory and no blocking peripheral calls are used.

## Timing

TIM6 runs from the 16 MHz APB1 clock with a 1 MHz counter:

```text
PSC = 15       → 1 MHz timer counter
ARR = N - 1    → N microseconds per update
```

The Protocol allows `N` from 1,000 through 60,000 and defines 5,000 as the PoC default. `SET_STREAM_CONFIG` is
accepted only in IDLE. The period register is updated atomically through the Platform API after range validation.

A telemetry frame carries 14 payload bytes and has 24 bytes total. At 115200 bps with 8-N-1, nominal wire time is
approximately 2.08 ms. The 1,000-microsecond contract minimum cannot be sustained continuously at this baud rate;
the static TX ring and overflow status make pressure visible, but final supported sustained rates require measured
hardware evidence and may require a contract or transport-profile decision.

## Static resource bounds

| Resource | Bound | Full behavior |
|---|---:|---|
| Protocol payload | 1024 bytes | Candidate rejected and parser resynchronizes |
| Encoded frame | 1034 bytes | Encoder rejects insufficient capacity |
| USART2 RX ring | 2048 bytes | New byte dropped; overflow counter increments |
| USART2 TX ring | 2048 bytes | Whole frame enqueue rejected; overflow counter increments |
| Protocol parser | One in-progress frame | CRC/length/timeout resynchronization |
| Pending timer ticks | `uint16` | Saturates; separate overflow counter increments |

## Authority boundary

`Protocol/` implements a pinned copy of the system repository's contract. The MCU repository owns implementation
choices such as buffer sizes and timer register mechanics, but not wire-level semantics. `DEVICE_STATUS` and
`ERROR_REPORT` emission behavior remains unimplemented until the shared contract supplies the missing trigger and
error-allocation rules.
