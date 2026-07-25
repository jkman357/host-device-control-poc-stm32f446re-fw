# Project Input — USART2 Loopback Diagnostic

## Project identity

| Field | Value |
|---|---|
| Project | Host-Device Control PoC — STM32F446RE USART2 Loopback Diagnostic |
| Repository | `jkman357/host-device-control-poc-stm32f446re-fw` |
| Source baseline | commit `dcffbd2` |
| Role | Temporary physical and software serial-path isolation test |
| Target board | ST NUCLEO-F446RE |
| Target MCU | STM32F446RET6 |

## Confirmed diagnostic inputs

| Input | Decision |
|---|---|
| PC tool | Tera Term |
| Physical connection | ST-LINK USB connector |
| Device transport | ST-LINK VCP bridged to USART2 PA2/PA3 |
| UART format | 115200 bps, 8 data bits, no parity, 1 stop bit, no flow control |
| Terminal local echo | Disabled |
| Runtime | Bare-metal event-driven superloop |
| RX/TX implementation | Existing interrupt-driven static ring buffers from `dcffbd2` |
| Echo behavior | Every received byte is queued back unchanged in main context |
| LED behavior | LD2 toggles after every successfully queued echo byte |
| TIM6 | Not started |
| Shared Protocol | Retained in source tree but bypassed at runtime |
| C# application | Not used in this diagnostic |

## Required minimum behavior

1. Receive a byte through USART2 RX interrupt and the static RX ring.
2. Transfer the pending UART event to main context.
3. Read the byte from the RX ring.
4. Queue the same byte into the TX ring without modification.
5. Transmit the byte through USART2 TXE interrupt.
6. Toggle LD2 after the byte is accepted by the TX ring.
7. Remain non-blocking and use no heap or RTOS service.

## Completion evidence

- STM32CubeIDE Debug build: zero errors and zero warnings.
- Firmware download through ST-LINK: successful.
- Tera Term `a` test: exactly one returned `a`.
- Tera Term `b` test: exactly one returned `b`.
- LD2 toggles for each returned character.
