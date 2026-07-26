# Changelog

## v0.2.5

- Added explicit minimal newlib syscall stubs for `_close`, `_lseek`, `_read`, and `_write`.
- Prevented STM32CubeIDE 2.2.0 / GNU Arm Embedded 14.3 from reporting a failed build after successful ELF generation because of `libnosys` warnings.
- Added validator and mutation-test coverage for the syscall boundary.
- Updated the independent Clang/LLD source list and firmware-reported patch version.

## v0.2.4

- Increased USART2 from 115200 to 460800 so the declared 1,000 Hz maximum is physically supportable.
- Corrected the 16 MHz USART2 BRR encoding to `0x0023` and added a compile-time consistency assertion.
- Added a compile-time telemetry bandwidth assertion with 20% reserve.
- Replaced unordered event batching with a bounded ordered RX-byte/tick/error queue.
- Added queue-ordering, queue-overflow, and app-level START/STOP boundary host tests.
- Added validator mutation tests and byte-exact Protocol test vectors.
- Made protocol command sweep strict by default and preserved frames following direct responses.
- Hardened FPU startup regression checks for address, masks, barriers and reset order.
- Kept the register-level, non-CubeMX implementation boundary.

## v0.2.3

- Enabled Cortex-M4F CP10/CP11 before entering main for hard-float telemetry.
