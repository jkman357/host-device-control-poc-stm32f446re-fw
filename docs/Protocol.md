# Binary Protocol v0.1.0

The authoritative project definition is `Protocol/Spec/Host_Device_Control_PoC_protocol.yaml`. This document is the human-readable implementation summary.

## Byte order

All multi-byte integers are little-endian.

## Frame

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | SOF0 = `0xA5` |
| 1 | 1 | SOF1 = `0x5A` |
| 2 | 1 | Wire version = `0x01` |
| 3 | 2 | Message ID (`uint16`) |
| 5 | 1 | Flags |
| 6 | 1 | Payload length, 0–48 |
| 7 | 2 | Sequence (`uint16`) |
| 9 | N | Payload |
| 9+N | 2 | CRC-16/CCITT-FALSE |

The total frame length is `11 + payload_length` bytes.

CRC input starts at `Wire version` and ends at the final payload byte. SOF and received CRC bytes are not included.

```text
Polynomial: 0x1021
Initial:    0xFFFF
RefIn:      false
RefOut:     false
XorOut:     0x0000
Check:      CRC("123456789") = 0x29B1
```

## Flags

A request is accepted only when its flags byte is exactly `0x01`.

| Value | Meaning |
|---:|---|
| `0x01` | Request |
| `0x02` | Response |
| `0x04` | Telemetry |

## Message allocation

| Range | Purpose |
|---|---|
| `0x0000–0x00FF` | Framework/health/device information |
| `0x0100–0x0FFF` | Application command/response |
| `0x2000–0x2FFF` | Telemetry/stream |

## Requests

All current request payloads are empty.

| ID | Name | Expected response |
|---:|---|---|
| `0x0001` | `PING_REQUEST` | `PING_RESPONSE` |
| `0x0002` | `GET_DEVICE_INFO_REQUEST` | `DEVICE_INFO_RESPONSE` |
| `0x0100` | `START_STREAM_REQUEST` | `START_STREAM_RESPONSE` |
| `0x0102` | `STOP_STREAM_REQUEST` | `STOP_STREAM_RESPONSE` |

The response sequence echoes the request sequence for correlation.

## Framework/device responses

### `ERROR_RESPONSE` (`0x00E0`)

Used when a request has invalid flags, an unsupported Message ID, or an invalid payload.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Rejected request Message ID |
| 2 | 1 | Result code |
| 3 | 1 | Current device state |

### `PING_RESPONSE` (`0x0081`)

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | Device uptime in ms, saturating |
| 4 | 1 | Device state |
| 5 | 1 | Wire protocol version |

### `DEVICE_INFO_RESPONSE` (`0x0082`)

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Wire protocol version |
| 1 | 1 | FW major |
| 2 | 1 | FW minor |
| 3 | 1 | FW patch |
| 4 | 1 | Board ID = 1 (`NUCLEO-F446RE`) |
| 5 | 1 | Transport ID = 1 (`USART2/ST-LINK VCP`) |
| 6 | 2 | Sample period in microseconds = 5000 |
| 8 | 1 | Maximum payload length = 48 |
| 9 | 1 | Capability flags |
| 10 | 2 | Reserved = 0 |

Capability flags:

- Bit 0: streaming
- Bit 1: CRC16
- Bit 2: event-driven firmware

## Application control responses

### `START_STREAM_RESPONSE` (`0x0101`)

### `STOP_STREAM_RESPONSE` (`0x0103`)

Both use the same two-byte payload:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Result code |
| 1 | 1 | Current device state |

Result codes:

| Value | Meaning |
|---:|---|
| 0 | OK |
| 1 | Invalid state |
| 2 | Unsupported |
| 3 | Invalid payload |
| 4 | Transport busy |

## `TELEMETRY` (`0x2000`)

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | Device uptime in ms, saturating |
| 4 | 2 | Signed sine sample (`int16`) |
| 6 | 1 | Device state |
| 7 | 1 | Status flags |
| 8 | 2 | Event-overflow count, saturated |
| 10 | 2 | UART RX overflow count, saturated |
| 12 | 2 | UART TX overflow count, saturated |

Telemetry frame length is 25 bytes. At 115200 bps, 8-N-1, its nominal wire occupancy is approximately 2.17 ms.

The header sequence is a modulo-65536 **sample-attempt** counter. It advances even when a telemetry frame cannot be queued. The host can therefore detect dropped sample attempts from sequence gaps.

## Parser behavior

- Partial serial reads are supported.
- Multiple frames in one serial read are supported.
- Invalid version/length increments the format-error counter and triggers resynchronization.
- CRC failure increments the CRC-error counter and triggers resynchronization.
- A valid frame with unsupported semantics receives `ERROR_RESPONSE`; malformed frames do not receive a response.
