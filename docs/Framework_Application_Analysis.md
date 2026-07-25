# Framework Application Analysis

## Scope mapping

| Framework concern | PoC realization |
|---|---|
| Coordinator/Node separation | This repository owns only the MCU firmware; PC software is separate. |
| Transport isolation | `Transport` owns USART2 buffering and interrupt service. |
| Protocol authority | System repository `protocol/protocol.yaml` is authoritative; MCU keeps a hash-pinned snapshot. |
| Application ownership | `App` owns command handling, state, sampling, and telemetry decisions. |
| Static resources | Parser/frame storage, event state, and RX/TX buffers have fixed capacities. |
| Failure visibility | Parser timeout/CRC/format and transport overflow/error diagnostics are retained. |
| Compatibility | Protocol version, device information, result codes, and lifecycle status are explicit. |
| Evidence boundary | Automated checks are separate from CubeIDE, physical-board, PC, and approval evidence. |

## Event model

```text
USART2 IRQ ──► RX ring / TX drain ──► UART_RX event
TIM6 IRQ   ──► configurable tick ───► TICK event
                                      │
                                      ▼
                             main-context dispatcher
                                      │
                 ┌────────────────────┼────────────────────┐
                 ▼                    ▼                    ▼
          shared parser         state machine       float telemetry
```

Interrupts do not execute Protocol command behavior. Main context drains published work and uses an atomic
check-and-sleep operation to avoid a lost wake-up race.

## Resource bounds

| Resource | Bound | Full behavior |
|---|---:|---|
| Protocol payload | 1024 bytes | Invalid length candidate discarded and parser resynchronized |
| Encoded frame storage | 1034 bytes | Encoder rejects insufficient caller capacity |
| USART2 RX ring | 2048 bytes | New byte dropped; saturating overflow counter increments |
| USART2 TX ring | 2048 bytes | Whole-frame enqueue rejected; saturating overflow counter increments |
| Pending timer ticks | `uint16` | Saturates; separate overflow counter increments |
| Protocol parser | One in-progress frame | Resynchronizes after length, CRC, or timeout failure |

## Timing budget

A `TELEMETRY_SAMPLE` frame is 24 bytes. At 115200 bps with 8-N-1 framing, nominal wire time is approximately
2.08 ms. The default 5 ms interval is nominally below wire capacity, but USB bridge buffering, host scheduling,
interrupt latency, and other frames still require measurement. The Protocol's 1 ms minimum interval is not
sustainable for continuous 24-byte telemetry on this UART profile.

## Contract and implementation boundary

The MCU implementation follows the system contract for framing, IDs, fields, results, states, and timeout
behavior. It does not claim authority to change them. Buffer size, register configuration, event representation,
and internal diagnostics remain MCU implementation choices as long as they preserve wire behavior.

The contract declares `DEVICE_STATUS` and `ERROR_REPORT`, but does not fully define their emission semantics.
Their IDs remain reserved; v0.2.1 does not invent MCU-local triggers or error-code meanings.

## Known gaps

- No v0.2.1 STM32CubeIDE clean-build evidence is included yet.
- No target-board execution or PC interoperability evidence is included.
- No measured ISR latency, jitter, sustained throughput, or packet-loss acceptance result is included.
- No approved PC compatible commit is pinned.
- No firmware update, security, multi-node, or reconnect/session-generation behavior is implemented.
- C source generation from YAML is manual; hash, ID, constants, vectors, and legacy-marker checks reduce drift.
