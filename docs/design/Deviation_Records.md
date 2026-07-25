# Deviation Records

## DR-NAME-001 — Toolchain-mandated entry and syscall symbols

| Field | Value |
|---|---|
| Affected symbols | `main`, `_close`, `_lseek`, `_read`, `_write`, `_estack`, `_sidata`, `_sdata`, `_edata`, `_sbss`, `_ebss` |
| Rule conflict | Product-owned functions normally require a module prefix and lower snake case |
| Cause | The C runtime, linker script, startup integration, and newlib-nano require these exact external symbol names |
| Alternatives evaluated | Wrapper-only names cannot replace the externally required linker symbols |
| Impact | Naming consistency is reduced only at the defined toolchain integration boundary |
| Controls | Declarations are isolated under `Core/Inc`; definitions contain complete Function Headers; linker symbols are declared without runtime storage ownership; syscall implementations reject unsupported operations; no dynamic allocation or hidden I/O is introduced |
| Verification | Host build, Cortex-M4 compile/link, zero-warning CubeIDE build, and automated symbol checks |
| Scope | This firmware PoC only |
| Status | Recorded for Project Owner review and acceptance |

No deviation in this record waives runtime behavior, error handling, or verification requirements.
