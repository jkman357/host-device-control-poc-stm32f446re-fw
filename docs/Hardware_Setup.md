# Hardware Setup

## Board

- ST NUCLEO-F446RE
- Target MCU: STM32F446RET6

## PC connection

Connect the PC to the board's ST-LINK USB connector. ST-LINK enumerates a Virtual COM Port and bridges it to USART2.

## Pins

| Function | MCU pin | NUCLEO use |
|---|---|---|
| USART2 TX | PA2 | ST-LINK VCP RX |
| USART2 RX | PA3 | ST-LINK VCP TX |
| Status LED | PA5 | LD2 |

## Serial settings

```text
Baud:      115200
Data bits: 8
Parity:    none
Stop bits: 1
Flow:      none
```

## Clock assumption

This firmware explicitly selects the internal 16 MHz HSI clock and leaves AHB/APB prescalers at 1. No external oscillator or PLL is required.


## Tera Term loopback settings

```text
Terminal:   Tera Term
Local echo: OFF
New-line:   any consistent setting for this byte-level test
```

With local echo disabled, a displayed character is evidence that the byte returned through the MCU. If local
echo is enabled, each typed character may appear twice.
