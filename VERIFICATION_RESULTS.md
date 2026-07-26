# Verification Results

Baseline: `064b5a71d466967d9ad6fb79ee2d31c0747f8c1f`  
Corrected firmware version: `0.2.5`  
Verification date: `2026-07-26`

## Completed software verification

The following command completed successfully from the repository root:

```sh
make validate
```

It executed:

- repository and protocol authority validation;
- byte-exact protocol-vector CRC validation;
- protocol round-trip and CRC rejection host test;
- ordered event queue and overflow host test;
- application-level START/STOP versus tick ordering host test;
- validator mutation regression tests;
- strict command-sweep parser/self-test, including preservation of unrelated and post-response frames;
- serial smoke compatibility self-test;
- linker layout test;
- independent Cortex-M4F hard-float Clang/LLD build;
- strict compilation and validator coverage of the explicit newlib syscall boundary.

Independent firmware image size from the final source tree:

```text
text: 5408 bytes
data: 0 bytes
bss: 6240 bytes
total: 11648 bytes
```

## Required hardware verification

The following evidence was not produced in this environment and remains required before release use:

1. STM32CubeIDE Debug and Release clean builds after the syscall-boundary correction, with zero `libnosys` diagnostics.
2. Flash and reset on the target NUCLEO-F446RE.
3. ST-LINK VCP interoperability at 460800, 8-N-1.
4. Strict command sweeps at 1000, 5000 and 60000 us.
5. Long-duration 1000 Hz streaming with zero sample gaps, status flags, CRC errors and format errors.
6. Recorded board identity, firmware revision, PC-tool revision, operator and approval.
