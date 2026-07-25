# Project Input

## Project identity

| Field | Value |
|---|---|
| Project | Host-Device Control PoC — STM32F446RE Firmware |
| Repository | `jkman357/host-device-control-poc-stm32f446re-fw` |
| Role | Device/Node firmware |
| Status | Draft PoC baseline |
| Target board | ST NUCLEO-F446RE |
| Target MCU | STM32F446RET6 |

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
| Application time base | TIM6 interrupt every 5 ms |
| Streaming sample | Signed 16-bit, 1 Hz sine lookup-table sample |
| Streaming rate | 200 samples/s |
| Device states | IDLE, STREAMING, FAULT |
| Integrity | CRC-16/CCITT-FALSE and modulo-65536 sequence |
| Security | Outside this direct-cable laboratory PoC scope |

## Required minimum behavior

1. Receive framed commands without blocking the main execution context.
2. Support `PING`, `GET_DEVICE_INFO`, `START_STREAM`, and `STOP_STREAM`.
3. Generate one telemetry sample attempt for every 5 ms event while streaming.
4. Report sequence and bounded diagnostic counters so the host can detect loss or overflow.
5. Keep interrupt service routines limited to transport/time-base work and event publication.
6. Keep application behavior out of the startup, register, and transport layers.

## Explicit constraints

- No RTOS API.
- No heap allocation.
- No blocking UART transmission.
- No application use of `HAL_Delay()`.
- Fixed-size buffers only.
- Interrupt/main shared data must have explicit synchronization.
- Protocol message identifiers are 16-bit and allocated by message domain.

## Open inputs requiring human closure

| Item | Current value |
|---|---|
| Exact framework commit | `7a68980ef5faa2e897a3574af121683d65f74638` |
| Approved PC application repository/commit | TBD |
| STM32CubeIDE version used for adoption | TBD |
| ARM GCC version used for adoption | TBD |
| Physical-board UART verification result | Not yet executed |
| Measured 5 ms timing/jitter | Not yet executed |
| Sustained streaming duration and acceptance limit | TBD |
| Permitted packet-loss/error threshold | TBD |
| Production hardware migration plan | Not applicable to this PoC baseline |
