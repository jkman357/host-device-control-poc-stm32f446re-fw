# USART2 Baud Rate Profiles

## Authority and selection model

`Transport/Inc/serial_baud.h` is the MCU-side authority for the supported UART list and the calculation rules. The firmware uses a **compile-time** baud selection. It does not change USART2 baud at runtime and it does not auto-detect the PC setting.

The default is `460800`. A build may override it globally with:

```text
SERIAL_TRANSPORT_BAUD_RATE=<selected value>u
```

The PC application and firmware image must use the same baud. Changing only the PC application's Baud field does not reconfigure an already-flashed MCU.

## Calculation rules

- USART2 peripheral clock: 16,000,000 Hz
- Format: 8-N-1, 10 wire bits per byte
- BRR: rounded integer divider, `(PCLK + baud / 2) / baud`
- Maximum accepted baud error: 25,000 ppm (2.5%)
- Telemetry frame: 24 bytes including framing and CRC
- Reserved line capacity: 20%
- Protocol interval range: 1,000 to 60,000 us
- 1,200 through 9,600 baud: command-only by policy

The minimum streaming interval is calculated from the conservative lower value of requested baud and actual baud, then rounded upward. The protocol's 1,000 us minimum is applied after the bandwidth calculation.

## Supported profiles

| Configured baud | BRR | Actual baud | Error | Mode | Minimum stream interval | Reported maximum rate |
|---:|---:|---:|---:|---|---:|---:|
| 1,200 | `0x3415` | 1,200.030 | 25 ppm | Command-only | N/A | 0 Hz |
| 2,400 | `0x1A0B` | 2,399.880 | 50 ppm | Command-only | N/A | 0 Hz |
| 4,800 | `0x0D05` | 4,800.480 | 100 ppm | Command-only | N/A | 0 Hz |
| 9,600 | `0x0683` | 9,598.080 | 200 ppm | Command-only | N/A | 0 Hz |
| 19,200 | `0x0341` | 19,207.683 | 400 ppm | Command + stream | 15,000 us | 66 Hz |
| 38,400 | `0x01A1` | 38,369.305 | 799 ppm | Command + stream | 7,507 us | 133 Hz |
| 57,600 | `0x0116` | 57,553.957 | 799 ppm | Command + stream | 5,005 us | 199 Hz |
| 115,200 | `0x008B` | 115,107.914 | 799 ppm | Command + stream | 2,503 us | 399 Hz |
| 230,400 | `0x0045` | 231,884.058 | 6,441 ppm | Command + stream | 1,250 us | 800 Hz |
| 460,800 | `0x0023` | 457,142.857 | 7,937 ppm | Command + stream | 1,000 us | 1,000 Hz |
| 921,600 | `0x0011` | 941,176.471 | 21,242 ppm | Command + stream | 1,000 us | 1,000 Hz |

At command-only baud rates, `GET_DEVICE_INFO` reports zero maximum stream rate. `SET_STREAM_CONFIG` and `START_STREAM` return `NACK / INVALID_STATE`. PING and GET_DEVICE_INFO remain available.

The 921,600 profile passes the configured 2.5% calculation limit but has the largest divider error. It must not be treated as qualified until target-board and ST-LINK VCP testing is recorded.
