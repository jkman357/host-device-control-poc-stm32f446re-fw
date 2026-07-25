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
