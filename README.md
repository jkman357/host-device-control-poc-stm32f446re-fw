# STM32F446RE Host–Device Control PoC Firmware

Bare-metal, event-driven firmware for the NUCLEO-F446RE. The ST-LINK Virtual COM Port connects to USART2 on PA2/PA3.

## Corrected baseline

This package is based on MCU commit `6b02740` and applies a source-code compliance refactor as firmware v0.2.8:

- USART2 supports the same 11 selectable baud values exposed by the PC application: 1,200, 2,400, 4,800, 9,600, 19,200, 38,400, 57,600, 115,200, 230,400, 460,800 and 921,600.
- MCU baud selection is compile-time; default is 460,800. The firmware does not auto-detect or dynamically follow the PC application's selection.
- BRR, actual baud and error are calculated from the reset-default 16 MHz USART2 peripheral clock. Every supported build is rejected at compile time if its divider error exceeds 2.5%.
- Each streaming baud has a calculated minimum stream interval with 20% reserved UART capacity.
- 1,200 through 9,600 baud are command-only. PING and GET_DEVICE_INFO remain available; SET_STREAM_CONFIG and START_STREAM return NACK/INVALID_STATE.
- CI independently builds all 11 firmware profiles and host tests verify the profile table and application enforcement.
- ISR-to-main transfer uses a fixed-capacity ordered event queue. RX bytes and TIM6 ticks retain their observed interrupt order, so START/STOP boundaries are deterministic.
- The synthetic signal rotates every 10 seconds of active streaming: sine → square → triangle → ECG at 70 bpm → sine.
- Explicit `_close`, `_lseek`, `_read`, and `_write` stubs prevent GCC 14/newlib-nano `libnosys` warnings from being promoted to a failed STM32CubeIDE build.
- Every Product-owned `.c` and `.h` file now has a standardized File Header.
- Every Product-owned function declaration and definition now has an immediate complete Function Header.
- Product source uses English comments, `//` for general comments, module-prefixed functions, named constants for system values, and no dynamic allocation.
- Protocol float serialization no longer uses union type-punning; it copies the verified object representation and writes explicit little-endian bytes.
- Unsolicited telemetry sequence rollover now returns to sequence 1 instead of emitting prohibited sequence 0.
- `Tools/check_coding_rules.py` provides a focused source gate without expanding documentation CI.

## Scope

- No RTOS and no heap allocation
- Cortex-M4F hard-float startup with early FPU enable
- USART2 through ST-LINK VCP, compile-time baud profile
- TIM6 interval from the baud-specific minimum through 60,000 us
- PING, GET_DEVICE_INFO, SET_STREAM_CONFIG, START_STREAM, STOP_STREAM
- ACK, NACK, DEVICE_INFO, TELEMETRY_SAMPLE
- One-second sine, square and triangle signals plus an 857,143 us synthetic ECG cycle (approximately 70 bpm)
- CRC-16/CCITT-FALSE, sequence correlation and sample-counter gap detection

## Baud selection

`Transport/Inc/serial_baud.h` is the MCU-side supported-list authority. The default build uses:

```c
#define SERIAL_TRANSPORT_BAUD_RATE SERIAL_BAUD_RATE_460800
```

For STM32CubeIDE, add one project-wide preprocessor symbol to every configuration and language settings set used by the build:

```text
SERIAL_TRANSPORT_BAUD_RATE=115200u
```

Then delete the old `Debug`/`Release` output, Refresh, Clean and rebuild. All firmware source files must receive the same define.

For the independent Clang build:

```sh
SERIAL_BAUD=115200 bash Tools/build_with_clang.sh
```

To compile every supported profile:

```sh
bash Tools/build_all_baud_profiles.sh
```

The PC application's Baud selection must match the flashed firmware image. Changing the PC field alone cannot change MCU USART2 configuration.

| Baud range | Mode | Stream rule |
|---|---|---|
| 1,200–9,600 | Command-only | GET_DEVICE_INFO reports 0 Hz; SET_STREAM_CONFIG and START_STREAM NACK |
| 19,200–230,400 | Command + stream | Baud-specific minimum interval is enforced |
| 460,800–921,600 | Command + stream | Protocol minimum 1,000 us is supported |

The complete BRR/error/interval matrix is in `docs/Baud_Rate_Profiles.md`.

## Waveform rotation rule

Each successful START_STREAM resets the waveform sequence to sine. While streaming, the firmware changes waveform every 10,000,000 us in this order: sine, square, triangle, synthetic ECG at 70 bpm, then back to sine. STOP_STREAM pauses generation; the next START_STREAM begins a new sequence from sine. The telemetry frame layout is unchanged, so the existing PC application can plot all four signals without a parser change.

The current system protocol authority still names the float field `sine_value`. Reusing that field for multiple synthetic waveforms is recorded as a temporary PoC semantic deviation. It must be aligned in the system protocol repository before promotion to a verified baseline.

## Event ordering rule

Every received UART byte and every TIM6 tick is inserted into one bounded queue in ISR-observed order. Main processes exactly one queue element at a time. A tick before the final byte of START_STREAM belongs to IDLE; a tick after it belongs to STREAMING. The same rule applies to STOP_STREAM.

## CubeMX boundary

This remains a Product-owned register-level project. It intentionally contains no `.ioc`, STM32 HAL, or generated CubeMX source. Do not run CubeMX generation over this tree.

## Newlib syscall boundary

This firmware has no POSIX file-descriptor service. `Core/Src/syscalls.c` supplies deterministic failing stubs for `_close`, `_lseek`, `_read`, and `_write`. They do not provide console or filesystem I/O.

## Build and validation

```sh
python3 Tools/check_coding_rules.py
python3 Tools/validate_project.py
make host-test
python3 Tests/test_validate_project.py
python3 Tools/protocol_command_sweep.py --self-test
python3 Tools/test_linker_layout.py
bash Tools/build_all_baud_profiles.sh
```

Hardware examples:

```sh
py -m pip install pyserial
py Tools\protocol_command_sweep.py --port COM3 --baud 460800 --interval-us 1000
py Tools\protocol_command_sweep.py --port COM3 --baud 9600
```

At 9,600 and below, the tool verifies command-only behavior. At streaming baud rates, it rejects an interval below the profile minimum and uses strict PASS criteria for sample gaps, status flags, CRC and format errors.

Software checks do not replace STM32CubeIDE clean-build evidence, board download, PC/MCU interoperability testing and human approval. Current target evidence and unexecuted rows are recorded in `docs/Hardware_Baud_Test_Record.md`.
