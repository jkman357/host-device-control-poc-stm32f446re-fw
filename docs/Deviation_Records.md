# Deviation Records

- Register-level peripheral definitions are intentionally local to keep the PoC independent of HAL/CMSIS packages.
- GNU range-designator syntax is used in the vector table and is accepted by the validated GCC/Clang toolchains.

- Firmware v0.2.7 temporarily reuses the existing `TELEMETRY_SAMPLE.sine_value` float as a generic synthetic waveform sample so the current PC application can display sine, square, triangle and ECG without a wire-layout change. The system protocol authority must rename or replace this semantic before the protocol can be promoted beyond candidate alignment.

- The PC application exposes runtime baud selection, while MCU v0.2.7 uses a compile-time baud profile. This is intentional for the PoC: the selected PC baud must match the flashed image, and dynamic baud-switch protocol support is outside the current authority.
