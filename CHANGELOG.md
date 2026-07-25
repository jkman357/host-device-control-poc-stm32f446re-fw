# Changelog

## v0.2.3 - 2026-07-25

- Enabled Cortex-M4F CP10 and CP11 access in the custom reset path before entering `main`.
- Fixed the runtime fault that occurred on the first hard-float sine-wave telemetry sample.
- Added DSB/ISB ordering after the CPACR update.
- Updated the interactive sweep to process one receive window before honoring immediate q/Q input.
- Added validator enforcement that FPU access is enabled before `main`.
- Updated the firmware-reported version to `0.2.3`; shared wire Protocol 0.1.0 is unchanged.

## Test tooling - 2026-07-25

- Added an interactive Python sweep for all implemented PC-to-MCU commands.
- Added continuous TELEMETRY_SAMPLE decoding and ASCII sine-wave display.
- Added immediate q/Q termination with confirmed STOP_STREAM ACK before closing the port.
- Added sample-gap, CRC, format, status-flag, and sine-range statistics.
- Kept firmware version 0.2.2 and shared wire Protocol 0.1.0 unchanged.

## v0.2.2 - 2026-07-25

- Fixed GitHub Actions exit code 127 in the Cortex-M4 Clang build step.
- Installed `clang`, `lld`, and `llvm` explicitly on the pinned Ubuntu 24.04 runner.
- Added explicit discovery and diagnostics for Clang, LLD, and LLVM objcopy tools.
- Updated the checkout action from Node.js 20-based `actions/checkout@v4` to `actions/checkout@v5`.
- Added validator checks for the CI toolchain contract and deprecated checkout regression.
- Recorded the successful v0.2.1 STM32CubeIDE build with zero errors and zero warnings.
- Updated the firmware-reported version to `0.2.2`; wire Protocol 0.1.0 is unchanged.

## v0.2.1 - 2026-07-25

- Removed two redundant TIM6 upper-bound comparisons that were always false for a `uint16_t` period.
- Preserved the hardware-valid period domain of 1 through 65,535 microseconds.
- Preserved the shared Protocol range of 1,000 through 60,000 microseconds at the application boundary.
- Added a repository regression check that rejects reintroduction of the redundant platform upper-bound guard.
- Updated the firmware-reported version to `0.2.1`.
- Superseded the v0.2.0 STM32CubeIDE result of zero errors and two warnings.

## v0.2.0 - 2026-07-25

- Replaced the MCU-local wire format with the pinned system-repository Protocol 0.1.0 contract.
- Changed Message ID to `uint8`, payload length to `uint16`, removed the flags field, and raised maximum payload to 1024 bytes.
- Added the 250 ms partial-frame timeout and decodable unsupported-version NACK behavior.
- Implemented shared `PING`, `GET_DEVICE_INFO`, `SET_STREAM_CONFIG`, `START_STREAM`, and `STOP_STREAM` commands.
- Implemented shared `ACK`, `NACK`, `DEVICE_INFO`, and `TELEMETRY_SAMPLE` messages.
- Replaced integer telemetry with `uint32` sample counter, `uint32` device tick, IEEE-754 float32 sine value, and `uint16` status flags.
- Added configurable 1,000–60,000 microsecond TIM6 sampling with a 5,000 microsecond default.
- Added exact shared JSON vectors and independent C/Python byte-level tests.
- Added a Protocol Authority Record and SHA-256 drift check for the pinned YAML snapshot.
- Preserved the v1.0.17 Embedded C Coding Rules application from MCU commit `4b1b701`.
- Reserved `DEVICE_STATUS` and `ERROR_REPORT` without inventing undefined emission semantics.

## v0.1.5 - 2026-07-25

- Applied Embedded C Coding Rules v1.0.17 to all Product-owned C and header files.
- Added approved File Headers, per-file copyright, and mandatory Function Headers.
- Renamed Product-owned functions to lower snake case with module prefixes.
- Added explicit application state-transition ownership and bounded HSI readiness polling.
- Replaced protocol, hardware, timing, capacity, and host-test magic values with named constants.
- Added checked transmit-result diagnostics and explicit host-test process-exit mapping.
- Added the Global Object Register and toolchain-symbol Deviation Record.
- Strengthened project validation for headers, comments, names, line length, prohibited APIs, protocol IDs, target metadata, and linker layout.
- Preserved the v0.1.4 CubeIDE target and linker configuration that built with zero errors and zero warnings.
- Updated the firmware-reported patch version to `0.1.5`.

## v0.1.4 - 2026-07-25

- Corrected the GNU ld LMA diagnostic that remained in v0.1.3.
- Removed the loadable `ram` program header.
- Assigned `.bss` and `._user_heap_stack` `NOLOAD` sections to `:NONE`.
- Added validation that rejects loadable zero-initialized RAM sections.
- Updated the firmware-reported patch version to `0.1.4`.

## v0.1.3 - 2026-07-25

- Attempted to correct the linker program-header layout after STM32CubeIDE/GNU ld reported `section ._user_heap_stack lma ... adjusted ...`.
- Assigned initialized RAM data to a dedicated `data` load segment and assigned `.bss` plus the reserved stack area to a separate `ram` load segment.
- This remained insufficient with GNU ld 14.3.1 because the `NOLOAD` sections inherited a Flash LMA; v0.1.3 is superseded by v0.1.4.
- Updated the firmware-reported patch version to `0.1.3`.
- Updated project validation to require the three-segment linker layout.

## v0.1.2 - 2026-07-25

- Added explicit unsupported `_close`, `_lseek`, `_read`, and `_write` syscall stubs for the bare-metal target.
- Removed the four newlib-nano/libnosys diagnostics that STM32CubeIDE 2.2.0 reported as build errors even though the ELF was produced.
- Split Flash and RAM into separate ELF program headers in the linker script.
- Removed the GNU linker `LOAD segment with RWX permissions` warning.
- Added validation for the syscall source and linker segment permissions.

## v0.1.1 - 2026-07-25

- Replaced the incomplete hand-authored CubeIDE target configuration from v0.1.0.
- Restored STM32CubeIDE target metadata derived from the original NUCLEO-F446RE project, including MCU, CPU, core, board, FPU, ABI, and Defaults records.
- Added Debug and Release managed-build configurations.
- Moved the linker script to the conventional project root.
- Removed generated `Debug/` content and package-only handoff documents.
- Kept project context under `docs/` instead of the repository root.
- Preserved the bare-metal event-driven application, protocol, and UART transport implementation.

## v0.1.0 - 2026-07-25

- Initial event-driven firmware PoC implementation.
- This package had incomplete STM32CubeIDE target metadata and could fail with `Unknown target`; it is superseded by v0.1.1.
