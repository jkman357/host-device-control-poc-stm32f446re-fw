# Firmware Architecture

## Runtime model

The firmware uses a non-blocking foreground dispatcher and interrupt-backed event sources.

```text
TIM6 ISR ───────────────┐
                        ├─> AppEvent pending state ─> main superloop ─> App_ProcessEvents()
USART2 RX/error ISR ────┘

main context:
  RX bytes → protocol parser → request dispatcher → device state
  5 ms ticks → uptime → sine sample → telemetry frame → TX ring
```

Interrupt handlers do not parse protocol frames, execute commands, calculate sine values, or build telemetry. They only move bytes, update bounded counters, and post coalesced event state.

## Responsibilities

### Core

- Reset and vector table
- C runtime data/BSS initialization
- Main event-dispatch loop

### Platform

- HSI 16 MHz clock selection
- PA5 LED setup
- PA2/PA3 USART alternate-function setup
- TIM6 5 ms interrupt setup
- NVIC and critical-section helpers

### Transport

- USART2 at 115200 bps, 8-N-1
- RX and TX static byte rings
- Interrupt-driven receive and transmit
- Overflow and UART error counters

### Protocol

- Frame synchronization
- Length validation
- CRC-16/CCITT-FALSE
- Encoding and byte-wise parsing
- Little-endian helper functions

### App

- Device state (`IDLE`, `STREAMING`, `FAULT`)
- Request handling
- Response construction
- 5 ms telemetry scheduling
- Sequence and diagnostic reporting
- LED heartbeat

## Concurrency boundaries

- USART2 ISR is the sole RX-ring producer.
- Main context is the sole RX-ring consumer.
- Main context is the sole TX-ring producer.
- USART2 ISR is the sole TX-ring consumer.
- Event state is exchanged through short PRIMASK-protected snapshots.
- No dynamic memory and no blocking peripheral calls are used.

## Timing

TIM6 runs from the 16 MHz APB1 clock:

```text
PSC = 15999 → 1 kHz timer counter
ARR = 4     → update every 5 counts = 5 ms
```

UART transmission is independent of sample creation. When the static TX ring cannot accept a complete frame, that telemetry attempt is dropped and the telemetry sequence still advances so the PC can detect a gap.
