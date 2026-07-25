# Build Verification

The package was checked for:

- valid `.project` and `.cproject` XML;
- presence of the original NUCLEO-F446RE target metadata fields required by STM32CubeIDE;
- no packaged `Debug/` or `Release/` generated directories;
- C protocol round-trip test;
- static repository validation;
- Cortex-M4 hard-float cross-compilation and link using Clang as an independent compiler check.

The final confirmation remains a clean import and Debug build in the user's installed STM32CubeIDE, followed by board-level testing.


## v0.1.2 linker diagnostic closure

The v0.1.1 CubeIDE build produced the ELF but emitted four libnosys syscall warnings and one RWX program-segment warning. Version v0.1.2 supplies explicit unsupported syscall stubs and separates Flash and RAM ELF program headers. The expected CubeIDE result after a clean rebuild is no `_close`/`_lseek`/`_read`/`_write` implementation diagnostic and no RWX LOAD-segment diagnostic.
## v0.1.3 linker LMA correction

The v0.1.2 CubeIDE build completed the ELF but GNU ld emitted `section ._user_heap_stack lma ... adjusted ...`. The cause was assigning initialized `.data`, zero-initialized `.bss`, and the reserved stack section to the same RAM program header even though `.data` has a Flash load address. Version v0.1.3 attempted to separate the sections into `flash`, `data`, and `ram` program headers. Actual STM32CubeIDE 2.2.0 / GNU ld 14.3.1 output showed that this was insufficient: `.bss` and `._user_heap_stack` still inherited a Flash LMA. The design is superseded by v0.1.4.



## v0.1.4 GNU ld NOLOAD correction

The v0.1.3 design still assigned `.bss` and `._user_heap_stack` to a dedicated `ram` `PT_LOAD` program header. GNU ld inherited the preceding Flash load-address progression for that header, so the output still showed `.bss` with a Flash LMA and emitted `section ._user_heap_stack lma ... adjusted ...`. Version v0.1.4 removes the `ram` loadable program header and assigns both `NOLOAD` sections to `:NONE`. They retain their RAM VMA, have RAM-equivalent LMA in section inspection, are not programmed from the image, and are initialized only by startup code or CPU stack use.

## v0.2.0 shared Protocol implementation

Version v0.2.0 replaces the earlier MCU-local wire format with the pinned system-repository Protocol 0.1.0
contract. Software verification includes:

- exact SHA-256 validation of the authoritative YAML snapshot;
- equality of all YAML and C Message IDs;
- validation of the one-byte Message ID, two-byte sequence, two-byte length, 1024-byte payload, and 250 ms timeout;
- exact C and Python reproduction of five shared vectors, including CRC;
- command/state/result implementation checks through review and host tests;
- independent Cortex-M4 Clang compile/link;
- GNU ld linker-layout regression.

The remaining confirmation is a zero-warning STM32CubeIDE clean build, firmware download, shared PC application
interoperability, sustained-rate measurement, pinned compatible commits, and human lifecycle approval.

## v0.2.1 zero-warning timer-range correction

The STM32CubeIDE 2.2.0 build of v0.2.0 completed with zero errors and two `-Wtype-limits` warnings in
`platform_start_sample_timer` and `platform_set_sample_period_us`. The parameter is `uint16_t`, so comparing it
with a hardware maximum of 65,535 microseconds is always false. Version v0.2.1 removes only that redundant upper
bound check. The meaningful hardware guard remains `period_us >= 1`, while the application layer continues to
enforce the shared Protocol range of 1,000 through 60,000 microseconds before calling the platform layer.

The remaining confirmation is a clean v0.2.1 STM32CubeIDE build showing zero errors and zero warnings.
