# Changelog

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
