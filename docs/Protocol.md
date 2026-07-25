# Shared Binary Protocol 0.1.0 — MCU Implementation Summary

The system repository's `protocol/protocol.yaml` is the wire-level authority. The copy under
`Protocol/Spec/Host_Device_Control_PoC_protocol.yaml` is a pinned snapshot used to implement and test this MCU
revision. See `Protocol_Authority_Record.md` for its SHA-256 and lifecycle boundary.

## Encoding and transport

- UART profile: 115200 bps, 8 data bits, no parity, 1 stop bit, no flow control
- Byte order: little-endian
- Text encoding: UTF-8
- Float encoding: IEEE-754 binary32, little-endian
- Maximum payload: 1024 bytes
- Partial-frame timeout: 250 ms

## Frame

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | SOF0 = `0xA5` |
| 1 | 1 | SOF1 = `0x5A` |
| 2 | 1 | Wire version = `0x01` |
| 3 | 1 | Message ID (`uint8`) |
| 4 | 2 | Sequence (`uint16`, little-endian) |
| 6 | 2 | Payload length (`uint16`, little-endian) |
| 8 | N | Payload, 0–1024 bytes |
| 8+N | 2 | CRC16 (`uint16`, little-endian) |

Total frame length is `10 + payload_length` bytes. There is no flags field.

CRC input begins with `version` and ends at the final payload byte. SOF and the stored CRC are excluded.

```text
Algorithm:   CRC-16/CCITT-FALSE
Polynomial:  0x1021
Initial:     0xFFFF
RefIn:       false
RefOut:      false
XorOut:      0x0000
Check:       CRC("123456789") = 0x29B1
```

## Sequence rules

- PC command sequence is a nonzero `uint16`.
- PC allocation increments monotonically and wraps from `0xFFFF` to `1`.
- A direct MCU response copies the request sequence.
- Unsolicited MCU messages use an independent `uint16` sequence.
- Telemetry loss detection primarily uses `TELEMETRY_SAMPLE.sample_counter`, a wrapping `uint32`.

## State model

| State | Value |
|---|---:|
| `idle` | `0x00` |
| `streaming` | `0x01` |

`START_STREAM` is valid only from IDLE. `STOP_STREAM` is valid only from STREAMING. Invalid transitions return
`NACK/INVALID_STATE`.

## Message IDs

| ID | Name | Direction | MCU behavior in this revision |
|---:|---|---|---|
| `0x01` | `PING` | PC → MCU | Implemented |
| `0x02` | `GET_DEVICE_INFO` | PC → MCU | Implemented |
| `0x03` | `SET_STREAM_CONFIG` | PC → MCU | Implemented |
| `0x04` | `START_STREAM` | PC → MCU | Implemented |
| `0x05` | `STOP_STREAM` | PC → MCU | Implemented |
| `0x80` | `ACK` | MCU → PC | Implemented |
| `0x81` | `NACK` | MCU → PC | Implemented |
| `0x82` | `DEVICE_INFO` | MCU → PC | Implemented |
| `0x83` | `DEVICE_STATUS` | MCU → PC | Defined; no emission trigger specified |
| `0x90` | `TELEMETRY_SAMPLE` | MCU → PC | Implemented |
| `0x91` | `ERROR_REPORT` | MCU → PC | Defined; error allocation/emission policy not specified |

## Commands and direct responses

### PING

- Payload: empty
- Allowed states: IDLE, STREAMING
- Success response: `ACK`

### GET_DEVICE_INFO

- Payload: empty
- Allowed states: IDLE, STREAMING
- Success response: `DEVICE_INFO`

### SET_STREAM_CONFIG

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | `interval_us`, valid range 1,000–60,000 |

The default is 5,000 microseconds. The command is accepted only in IDLE and reconfigures TIM6 after validation.

### START_STREAM

- Payload: empty
- Allowed state: IDLE
- Success effect: enter STREAMING and reset stream counters
- Success response: `ACK`

### STOP_STREAM

- Payload: empty
- Allowed state: STREAMING
- Success effect: enter IDLE and stop telemetry
- Success response: `ACK`

### ACK and NACK

Both have a three-byte payload:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 1 | Request Message ID |
| 1 | 1 | Result code |
| 2 | 1 | Current device state |

For `ACK`, result code is required to be `OK (0x00)`.

## DEVICE_INFO payload

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | Device type = `0x4460` |
| 2 | 1 | Firmware major |
| 3 | 1 | Firmware minor |
| 4 | 1 | Firmware patch |
| 5 | 2 | Maximum stream rate in Hz |
| 7 | 1 | Device name length |
| 8 | N | UTF-8 device name, maximum 32 bytes |

This implementation reports device name `NUCLEO-F446RE` and firmware `0.2.1`.

## TELEMETRY_SAMPLE payload

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | `sample_counter` (`uint32`) |
| 4 | 4 | `device_tick_us` (`uint32`) |
| 8 | 4 | `sine_value` (IEEE-754 float32) |
| 12 | 2 | `status_flags` (`uint16`) |

Payload size is exactly 14 bytes. The sample counter starts at one after a successful `START_STREAM` and wraps as
`uint32`. The sine source is a 1 Hz, 1000-entry float lookup table indexed by the configured interval phase.

## Result codes

| Value | Name |
|---:|---|
| `0x00` | `OK` |
| `0x01` | `INVALID_COMMAND` |
| `0x02` | `INVALID_LENGTH` |
| `0x03` | `INVALID_VALUE` |
| `0x04` | `INVALID_STATE` |
| `0x05` | `UNSUPPORTED_VERSION` |
| `0x06` | `INTERNAL_ERROR` |

## Error and resynchronization behavior

- Invalid CRC: discard candidate and search for the next SOF; no response.
- Invalid payload length above 1024: discard candidate and search for the next SOF; no response.
- Partial frame older than 250 ms: discard partial frame.
- Decodable unsupported version: send `NACK/UNSUPPORTED_VERSION`.
- Decodable unknown Message ID: send `NACK/INVALID_COMMAND`.
- Invalid state: send `NACK/INVALID_STATE`.
- PC command sequence zero: send `NACK/INVALID_VALUE`.

## Shared vectors

The normative JSON vectors under `Protocol/TestVectors/` cover:

- PC command: `PING`
- MCU response: `ACK`
- Configuration command: `SET_STREAM_CONFIG`
- State command: `START_STREAM`
- MCU event: `TELEMETRY_SAMPLE`

Both C and Python tests compare every encoded byte, including CRC. A same-language round trip alone is not treated
as sufficient interoperability evidence.
