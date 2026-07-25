# PoC Verification Plan

## Static checks

- Required repository and Protocol authority files exist.
- The pinned YAML SHA-256 matches the recorded system-repository snapshot.
- C Message IDs match all YAML Message IDs.
- Frame constants match one-byte Message ID, two-byte sequence, two-byte length, 1024-byte maximum payload, and 250 ms timeout.
- Shared JSON vectors reproduce exact bytes and CRC.
- Legacy MCU-local frame markers are absent.
- No `malloc`, `calloc`, `realloc`, or `free` use.
- No RTOS API, blocking HAL delay, or blocking HAL UART use.
- Coding Rules File Headers, Function Headers, names, line length, and target metadata pass.
- GNU ld layout has no load-address adjustment and no RWE segment.

## Host-side shared-vector tests

`make host-test` verifies in C:

- CRC-16/CCITT-FALSE check value.
- Exact encoding of all shared vectors.
- Byte-by-byte parsing of all shared vectors.
- Message ID, sequence, length, and payload preservation.
- CRC rejection.
- Partial-frame timeout behavior.

`python3 Tools/serial_smoke_test.py --self-test` independently verifies the same normative vectors and parser
behavior in Python. Passing only an encoder/decoder pair from one implementation is not considered sufficient.

## STM32CubeIDE target checks

1. Delete generated `Debug/` and `Release/` directories.
2. Refresh or freshly import the repository.
3. Clean-build Debug with zero errors and zero warnings.
4. Record STM32CubeIDE and GNU Arm toolchain versions.
5. Confirm ELF, MAP, LIST, size, and linker layout outputs.

## Hardware interoperability sequence

1. Program the NUCLEO-F446RE through ST-LINK.
2. Confirm the ST-LINK VCP appears on the PC.
3. Open the port at 115200, 8-N-1, no flow control.
4. Send `PING`, sequence 1, and verify `ACK` sequence 1 with payload `01 00 00`.
5. Send `GET_DEVICE_INFO` and verify device type `0x4460`, firmware version, maximum rate, and UTF-8 name.
6. Send `SET_STREAM_CONFIG` with 5,000 microseconds in IDLE and verify `ACK`.
7. Send `START_STREAM` and verify `ACK`, STREAMING state, and reset stream counters.
8. Verify `TELEMETRY_SAMPLE` has 14 payload bytes and valid float32 decoding.
9. Verify `sample_counter` starts at one and increments modulo `uint32`.
10. Verify `device_tick_us` increments by the configured interval modulo `uint32`.
11. Verify the sine signal completes approximately one cycle per second.
12. Send `STOP_STREAM`, verify `ACK` with IDLE state, and verify telemetry stops.
13. Send `STOP_STREAM` again and verify `NACK/INVALID_STATE`.
14. Send `SET_STREAM_CONFIG` while STREAMING and verify `NACK/INVALID_STATE`.
15. Send interval values 999 and 60,001 and verify `NACK/INVALID_VALUE`.
16. Send a command with sequence zero and verify `NACK/INVALID_VALUE`.
17. Send a decodable unsupported version and verify `NACK/UNSUPPORTED_VERSION`.
18. Send an unknown Message ID and verify `NACK/INVALID_COMMAND`.
19. Corrupt a command CRC and verify no response plus parser resynchronization.
20. Interrupt a partial frame for more than 250 ms and verify it is discarded.
21. Observe LD2: off in IDLE and toggling every 500 ms in STREAMING.

## Rate and timing evidence

PC serial callbacks can deliver partial frames or multiple frames together. Use payload fields and captured wire
bytes rather than callback arrival intervals alone.

At 115200 bps, one 24-byte telemetry frame occupies about 2.08 ms on the wire. The Protocol permits a 1,000
microsecond interval, but this transport cannot sustain one complete 24-byte frame every millisecond. Verification
shall identify the sustained loss-free interval range and retain overflow/status evidence. The contract's maximum
stream-rate value must not be treated as measured performance evidence.

## Lifecycle exit criteria

MCU completion does not promote the contract. `verified_baseline` requires:

- matching PC and MCU implementations;
- passing shared vectors on both sides;
- physical hardware interoperability evidence;
- pinned compatible commits;
- recorded human approval in the system repository.
