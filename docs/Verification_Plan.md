# PoC Verification Plan

## Static checks

- Required repository files exist.
- No nested legacy `poc-446re/` directory exists.
- No `malloc`, `calloc`, `realloc`, or `free` use.
- No RTOS API use.
- No blocking HAL delay or UART API use.
- Main uses the event dispatcher.

## Host-side protocol test

`make host-test` verifies:

- Frame encoding.
- Byte-by-byte parsing.
- Header field preservation.
- Payload preservation.
- CRC error detection.

## Target checks

1. Clean build without warnings.
2. Program NUCLEO-F446RE through ST-LINK.
3. Confirm VCP appears on the PC.
4. Open at 115200 8-N-1.
5. Send a valid PING request and verify `PING_RESPONSE` with the same sequence.
6. Send GET_DEVICE_INFO and verify protocol/FW/board/transport fields.
7. Send START_STREAM from IDLE and verify result OK.
8. Verify telemetry sequence increments at 200 samples per second.
9. Verify device timestamp increments by 5 ms per telemetry sample.
10. Verify one complete 200-sample cycle represents a 1 Hz sine wave.
11. Send STOP_STREAM and verify telemetry stops.
12. Corrupt one command CRC and verify the command is ignored.
13. Send an unsupported message ID and verify result code `UNSUPPORTED`.
14. Observe LD2: off in IDLE, toggling every 500 ms while STREAMING.

## Timing interpretation

PC serial receive callbacks may deliver partial frames or multiple frames together. Timing verification must use the device timestamp and telemetry sequence, not the arrival interval of individual PC read callbacks.

- GNU ld linker-layout regression: `.bss` and `._user_heap_stack` shall have equal RAM VMA/LMA, produce no LMA adjustment diagnostic, and create no RWE segment.
