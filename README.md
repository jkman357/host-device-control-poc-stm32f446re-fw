# STM32F446RE Host–Device Control PoC Firmware

Bare-metal, event-driven firmware for the NUCLEO-F446RE. The ST-LINK Virtual COM Port connects to USART2 on PA2/PA3.

## Corrected baseline

This package is based on MCU commit `064b5a7` and closes the review findings:

- USART2 is `460800, 8-N-1`, using BRR `0x0023` at the reset-default 16 MHz peripheral clock and providing enough capacity for 1,000 telemetry frames/s plus 20% reserve.
- ISR-to-main transfer uses a fixed-capacity ordered event queue. RX bytes and TIM6 ticks retain their observed interrupt order, so START/STOP boundaries are deterministic.
- `Tools/protocol_command_sweep.py` uses strict PASS criteria and retains frames received after a direct response.
- Startup validation checks the CPACR address, CP10/CP11 masks, DSB/ISB barriers, and reset-handler ordering.
- Host tests cover Protocol round-trip/CRC, queue ordering/overflow, and START/STOP tick boundaries.
- Validator regression tests prove that FPU, baud, IRQ-priority, and sweep-strictness regressions are rejected.
- Byte-exact frames under `Protocol/TestVectors/` are CRC-checked by the validator.
- Explicit `_close`, `_lseek`, `_read`, and `_write` stubs prevent GCC 14/newlib-nano `libnosys` warnings from being promoted to a failed STM32CubeIDE build.

## Scope

- No RTOS and no heap allocation
- Cortex-M4F hard-float startup with early FPU enable
- USART2 at 460800 through ST-LINK VCP
- TIM6 interval from 1,000 to 60,000 us
- PING, GET_DEVICE_INFO, SET_STREAM_CONFIG, START_STREAM, STOP_STREAM
- ACK, NACK, DEVICE_INFO, TELEMETRY_SAMPLE
- CRC-16/CCITT-FALSE, sequence correlation and sample-counter gap detection

## Event ordering rule

Every received UART byte and every TIM6 tick is inserted into one bounded queue in ISR-observed order. Main processes exactly one queue element at a time. A tick before the final byte of START_STREAM belongs to IDLE; a tick after it belongs to STREAMING. The same rule applies to STOP_STREAM.

## Bandwidth rule

A telemetry frame is 24 bytes including framing and CRC. At 1,000 samples/s with 8-N-1, the stream requires 240,000 bit/s. The firmware uses 460,800 baud and enforces a compile-time assertion with 20% reserve. PC tools must open the VCP at 460800.

## CubeMX boundary

This remains a Product-owned register-level project. It intentionally contains no `.ioc`, STM32 HAL, or generated CubeMX source. Do not run CubeMX generation over this tree.

## Newlib syscall boundary

This firmware has no POSIX file-descriptor service. `Core/Src/syscalls.c` therefore supplies deterministic failing stubs for `_close`, `_lseek`, `_read`, and `_write`. They exist only to prevent `nosys.specs` from pulling warning-bearing `libnosys` stubs during linking; they do not provide console or filesystem I/O.

## Build and validation

```sh
python3 Tools/validate_project.py
make host-test
python3 Tests/test_validate_project.py
python3 Tools/protocol_command_sweep.py --self-test
python3 Tools/test_linker_layout.py
bash Tools/build_with_clang.sh
```

Hardware sweep:

```sh
py -m pip install pyserial
py Tools\protocol_command_sweep.py --port COM3
```

Press `q` or `Q` to stop. The default mode fails on sample gaps, non-zero status, CRC errors, format errors, no samples, or a first sample counter other than one. `--statistics-only` is an explicit diagnostic mode and must not be used as release evidence.

Software checks do not replace STM32CubeIDE clean-build evidence, board download, PC/MCU interoperability testing, and human approval.
