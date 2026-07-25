# STM32F446RE Host–Device Control PoC Firmware

Bare-metal, event-driven firmware for the **NUCLEO-F446RE**. The PC communicates through the board's
ST-LINK Virtual COM Port, which connects to STM32 USART2 on PA2 and PA3.

## Scope

- No RTOS and no heap allocation
- Event-driven superloop with bounded interrupt-to-main event transfer
- USART2 at 115200, 8-N-1
- Interrupt-driven RX and TX ring buffers
- TIM6-generated five-millisecond application event
- `PING`, `GET_DEVICE_INFO`, `START_STREAM`, and `STOP_STREAM` commands
- 200 Hz signed 16-bit sine-wave telemetry
- Framing, sequence number, and CRC-16/CCITT-FALSE

## Coding-rules baseline

Firmware v0.1.5 applies `Embedded_C_Coding_Rules.md` v1.0.17 from framework commit
`7a68980ef5faa2e897a3574af121683d65f74638`.

Applied controls include:

- Standard File Headers and per-file copyright
- Mandatory Function Headers on declarations and definitions
- Lower snake case function names with module prefixes
- `s_` file-static and `g_` global object naming
- Explicit Global Object and Deviation records
- Named constants for protocol, hardware, timing, capacity, and test values
- Checked return values and explicit process-exit mapping in host tests
- Static resources, bounded loops, bounded ISR work, and zero-warning build gates
- Automated mechanical validation in `Tools/validate_project.py`

See [`Coding_Rules_Application_Report.md`](docs/compliance/Coding_Rules_Application_Report.md).
This Project-specific baseline does not by itself establish MISRA C:2023 compliance.

## STM32CubeIDE import and build

Use a fresh import when replacing an earlier package:

1. Remove the old project from the workspace without deleting the repository `.git/` directory.
2. Delete locally generated `Debug/` and `Release/` directories.
3. Replace the repository working tree with this package while keeping `.git/`.
4. Select **File → Import → Existing Projects into Workspace**.
5. Select the repository root and import `host-device-control-poc-stm32f446re-fw`.
6. Select **Project → Clean**, then build the **Debug** configuration.

The project metadata retains the validated NUCLEO-F446RE target, Cortex-M4F, hard-float, and linker settings.

## Implementation boundary

This PoC uses a small Product-owned register-level platform layer. It does not include STM32 HAL/CMSIS source
packages or a CubeMX `.ioc` file. Do not run CubeMX code generation on this project. A future HAL or LL migration
should replace the internals of `Platform/` and `Transport/` without moving application or protocol ownership.

The wire protocol implemented by this firmware remains the repository protocol baseline. Alignment to the
separately supplied shared PC/MCU `protocol.yaml` is a separate controlled protocol change and is not included in
this coding-rules-only revision.

## Runtime flow

```text
TIM6 IRQ ──> pending five-millisecond tick count ─┐
USART2 IRQ ──> RX/TX ring buffers ────────────────┼─> main-context event processing
UART error ──> event flag ────────────────────────┘
```

Interrupt handlers only move bytes, acknowledge hardware, update bounded counters, and post events. Parsing,
command handling, state transitions, telemetry construction, and sine generation execute in main context.

## Validation

```bash
python3 Tools/validate_project.py
make host-test
python3 Tools/serial_smoke_test.py --self-test
python3 Tools/test_linker_layout.py
bash Tools/build_with_clang.sh
```

`Tools/validate_project.py` checks high-value mechanical rules but does not replace human review or static analysis.

## Documentation

- [`Architecture.md`](docs/Architecture.md)
- [`Protocol.md`](docs/Protocol.md)
- [`Hardware_Setup.md`](docs/Hardware_Setup.md)
- [`Verification_Plan.md`](docs/Verification_Plan.md)
- [`Global_Object_Register.md`](docs/design/Global_Object_Register.md)
- [`Deviation_Records.md`](docs/design/Deviation_Records.md)

## Status

Draft PoC baseline v0.1.5. Software checks do not replace an STM32CubeIDE clean build, firmware download, and
validation on physical NUCLEO-F446RE hardware.
