# Changelog

## v0.2.7

- Added the shared MCU-side list for 1,200 through 921,600 baud, matching the 11 PC application choices.
- Added compile-time `SERIAL_TRANSPORT_BAUD_RATE` selection with 460,800 as the default.
- Added USART2 BRR, actual-baud and ppm-error calculations for the 16 MHz peripheral clock.
- Added compile-time rejection for unsupported baud values, invalid BRR values and error above 2.5%.
- Added baud-dependent minimum stream intervals with 20% reserved line capacity.
- Added command-only behavior at 1,200, 2,400, 4,800 and 9,600 baud.
- Updated DEVICE_INFO to report the selected profile's effective maximum stream rate.
- Added host tests for all profile calculations and application-level policy enforcement.
- Added CI builds for all 11 baud profiles and extended the strict hardware sweep tool.
- Added baud profile documentation and an explicit hardware-test record with prior evidence separated from unexecuted v0.2.7 qualification.
- Updated the firmware-reported patch version to 0.2.7.

## v0.2.6

- Replaced the cusp-producing sine approximation with a smooth range-reduced seventh-order polynomial.
- Added square, triangle and synthetic ECG waveforms.
- Added automatic 10-second rotation: sine → square → triangle → ECG 70 bpm → sine.
- Reset waveform rotation to sine on every successful START_STREAM.
- Added waveform shape and application-level rotation host tests.
- Added validator and mutation-test coverage for waveform generation and the 10-second switching rule.
- Kept the existing 14-byte TELEMETRY_SAMPLE payload for PC interoperability and documented the temporary protocol-semantic deviation.
- Updated the firmware-reported patch version to 0.2.6.

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
