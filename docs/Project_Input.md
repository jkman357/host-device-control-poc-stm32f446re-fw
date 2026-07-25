# Project Input

## Project identity

| Field | Value |
|---|---|
| Project | Host-Device Control PoC — STM32F446RE Firmware |
| Repository | `jkman357/host-device-control-poc-stm32f446re-fw` |
| MCU implementation baseline | commit `4b1b701` |
| Role | Device/Node firmware and shared-Protocol consumer |
| Status | Draft MCU implementation baseline v0.2.1 |
| Target board | ST NUCLEO-F446RE |
| Target MCU | STM32F446RET6 |

## Protocol input

| Field | Decision |
|---|---|
| Authority repository | `jkman357/host-device-control-poc-system` |
| Authority path | `protocol/protocol.yaml` |
| Protocol version | `0.1.0` |
| Wire version | `0x01` |
| Contract status | `candidate_for_alignment` |
| Authority rule | `specification_precedes_implementation` |
| Snapshot SHA-256 | `7ff8db3a1ed669407e0d4cada2a78b212ea3c7bccdf371f232a2689a02e7c56e` |

## Confirmed PoC inputs

| Input | Decision |
|---|---|
| Host | PC application, implemented in a separate repository |
| Physical connection | ST-LINK USB connector |
| Device transport | ST-LINK VCP bridged to USART2 PA2/PA3 |
| UART format | 115200 bps, 8 data bits, no parity, 1 stop bit |
| Topology | One host to one device, connection-bound identity |
| Runtime | Bare-metal event-driven superloop |
| RTOS | None |
| Dynamic memory | Prohibited |
| Application time base | TIM6, configurable from 1,000 to 60,000 microseconds |
| Default stream interval | 5,000 microseconds |
| Streaming sample | IEEE-754 float32, 1 Hz sine lookup-table sample |
| Device states | IDLE and STREAMING |
| Integrity | CRC-16/CCITT-FALSE, frame sequence, and telemetry sample counter |
| Security | Outside this direct-cable laboratory PoC scope |

## Required minimum behavior

1. Receive shared framed commands without blocking the main execution context.
2. Support `PING`, `GET_DEVICE_INFO`, `SET_STREAM_CONFIG`, `START_STREAM`, and `STOP_STREAM`.
3. Generate one `TELEMETRY_SAMPLE` attempt for each configured timer event while streaming.
4. Return `ACK`, `NACK`, or `DEVICE_INFO` with the request sequence as defined by the contract.
5. Report the `uint32` sample counter, `uint32` device tick, float32 sine value, and status flags.
6. Keep interrupt service routines limited to transport/time-base work and event publication.
7. Keep application behavior out of startup, register, and transport layers.
8. Match the shared byte-level vectors in both encoder and parser tests.

## Explicit constraints

- No RTOS API.
- No heap allocation.
- No blocking UART transmission.
- No application use of `HAL_Delay()`.
- Fixed-size buffers only.
- Interrupt/main shared data must have explicit synchronization.
- Protocol Message ID is `uint8`.
- Protocol payload length and sequence are little-endian `uint16`.
- Maximum protocol payload is 1024 bytes.
- MCU implementation must not supersede the system-repository Protocol authority.

## Open inputs requiring human closure

| Item | Current value |
|---|---|
| Approved PC application compatible commit | TBD |
| STM32CubeIDE clean-build result for v0.2.1 | Not yet executed |
| Physical-board command/response result | Not yet executed |
| Physical-board telemetry result | Not yet executed |
| Measured timing/jitter by configured interval | Not yet executed |
| Sustained supported stream-rate range at 115200 bps | TBD; 1 kHz is not wire-sustainable |
| Permitted packet-loss/error threshold | TBD |
| `DEVICE_STATUS` trigger semantics | Not defined by Protocol 0.1.0 |
| `ERROR_REPORT` error-code allocation and policy | Not defined by Protocol 0.1.0 |
| Contract lifecycle promotion | Requires PC/MCU/hardware/pinned-commit/human evidence |
