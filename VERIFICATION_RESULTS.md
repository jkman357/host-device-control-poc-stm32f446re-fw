# Verification Results

Baseline firmware commit: `04f8419`

Corrected firmware version: `0.2.7`

Verification date: `2026-07-26`

## Completed software verification

The repository software checks cover:

- repository and protocol authority validation;
- byte-exact protocol-vector CRC validation;
- protocol round-trip and CRC rejection host test;
- ordered event queue and overflow host test;
- application-level START/STOP versus tick ordering host test;
- waveform shape and 10-second rotation tests;
- shared 11-value baud list, BRR, actual-rate error and interval calculations;
- command-only application behavior at 9,600 baud;
- stream minimum enforcement at 19,200, 460,800 and 921,600 baud;
- strict command-sweep policy self-test for all baud profiles;
- validator mutation regression tests;
- linker layout test;
- independent Cortex-M4F hard-float Clang/LLD builds for all 11 baud profiles.

Run from the repository root:

```sh
make validate
```

## Hardware evidence status

One prior-baseline screenshot was provided for firmware 0.2.5 at 460,800 baud and 200 Hz. It showed 1,090 samples with zero CRC/format/partial/unknown errors and zero lost/UI drops. This is retained under `docs/evidence/` and recorded as continuity evidence only.

No v0.2.7 target-board profile is claimed as executed in this package. The following remain required:

1. STM32CubeIDE Debug and Release clean builds for each selected baud profile.
2. Flash and reset on the target NUCLEO-F446RE.
3. Command-only checks at 1,200, 2,400, 4,800 and 9,600.
4. Strict streaming sweeps at every profile-specific minimum interval.
5. Long-duration 1,000 Hz testing at 460,800 and 921,600.
6. Explicit 921,600 ST-LINK VCP stability evidence because its calculated divider error is 2.1242%.
7. Visual confirmation of at least one complete 40-second waveform sequence.
8. System-protocol authority alignment for generic synthetic sample semantics.
9. Completed board, tool, operator and approval fields in `docs/Hardware_Baud_Test_Record.md`.
