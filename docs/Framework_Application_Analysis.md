# Framework Application Analysis

## Scope mapping

| Framework concern | PoC realization |
|---|---|
| Coordinator/Node separation | This repository owns only the Node firmware; PC software is separate. |
| Transport isolation | `Transport` owns USART2 buffering and interrupt service. |
| Protocol authority | `Protocol/Spec/Host_Device_Control_PoC_protocol.yaml` owns message IDs and payload contracts. |
| Application ownership | `App` owns command handling, state, sampling, and telemetry decisions. |
| Static resources | Event flags/counters and RX/TX buffers have fixed capacities. |
| Failure visibility | Parser errors and event/RX/TX overflows are counted; telemetry exposes bounded counters. |
| Compatibility | Protocol version and device information are queryable. |
| Evidence boundary | Automated checks are recorded separately from physical-board verification. |

## Event model

```text
USART2 IRQ ──► RX ring / TX drain ──► UART_RX event
TIM6 IRQ   ──► 5 ms tick counter   ──► TICK event
                                      │
                                      ▼
                             main-context dispatcher
                                      │
                 ┌────────────────────┼────────────────────┐
                 ▼                    ▼                    ▼
             parser             state machine          telemetry
```

Interrupts do not execute protocol command behavior. The main context drains published work, then uses an atomic check-and-sleep operation to avoid a lost wake-up race.

## Resource bounds

| Resource | Bound | Full behavior |
|---|---:|---|
| Protocol payload | 48 bytes | Encoder rejects larger payloads |
| USART2 RX ring | 256 bytes | New byte dropped; saturated overflow counter increments |
| USART2 TX ring | 512 bytes | Frame enqueue rejected; saturated overflow counter increments |
| Pending 5 ms ticks | 32-bit saturating counter | Overflow counter records saturation attempts |
| Protocol parser | One in-progress frame | Resynchronizes on SOF after format/CRC errors |

## Timing budget

A telemetry frame is 25 bytes. At 115200 bps with 8-N-1 framing, its nominal wire time is approximately 2.17 ms, below the 5 ms sample period. This does not establish physical-board timing compliance; USB bridge buffering and host scheduling still require measurement.

## Known gaps

- No target-board execution evidence is included.
- No measured ISR latency, sample jitter, sustained throughput, or packet-loss acceptance result is included.
- No PC application compatibility result is included.
- No firmware update, security, multi-node, or reconnect/session-generation behavior is implemented.
- Protocol source generation is manual in this baseline; the YAML and C constants are checked for ID consistency only.
