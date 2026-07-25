# STM32F446RE Host–Device Control PoC Firmware

Bare-metal, event-driven firmware for the **NUCLEO-F446RE**. The PC communicates through the board's
ST-LINK Virtual COM Port, which connects to STM32 USART2 on PA2 and PA3.

## Scope

- No RTOS and no heap allocation
- Event-driven superloop with bounded interrupt-to-main event transfer
- USART2 at 115200, 8-N-1
- Interrupt-driven RX and TX static ring buffers
- Configurable TIM6 sample interval from 1,000 to 60,000 microseconds
- Shared `PING`, `GET_DEVICE_INFO`, `SET_STREAM_CONFIG`, `START_STREAM`, and `STOP_STREAM` commands
- Shared `ACK`, `NACK`, `DEVICE_INFO`, and `TELEMETRY_SAMPLE` MCU messages
- IEEE-754 float32 sine-wave telemetry
- Shared framing, sequence, partial-frame timeout, and CRC-16/CCITT-FALSE rules

## Protocol authority

The wire-level source of truth is maintained by the system repository:

- Repository: `jkman357/host-device-control-poc-system`
- Authoritative path: `protocol/protocol.yaml`
- Protocol version: `0.1.0`
- Wire version: `0x01`
- Contract status: `candidate_for_alignment`
- Authority rule: `specification_precedes_implementation`

This repository contains a pinned implementation snapshot at
[`Protocol/Spec/Host_Device_Control_PoC_protocol.yaml`](Protocol/Spec/Host_Device_Control_PoC_protocol.yaml).
The snapshot SHA-256 and source baseline are recorded in
[`Protocol_Authority_Record.md`](docs/Protocol_Authority_Record.md).

The MCU implementation must not independently redefine framing, IDs, field widths, payload semantics, result
codes, state transitions, or compatibility rules. A later change begins in the system-repository contract and
shared vectors, then propagates to both MCU and PC implementations.

The contract remains `candidate_for_alignment`. This MCU implementation alone does not promote it to
`implementation_aligned` or `verified_baseline`; promotion also requires the matching PC implementation,
hardware interoperability evidence, pinned commits, and recorded human approval.

## Shared wire format

```text
A5 5A | version:u8 | message_id:u8 | sequence:u16 LE |
payload_length:u16 LE | payload:0..1024 | crc16:u16 LE
```

CRC covers `version` through the final payload byte and excludes SOF. Direct responses copy the request sequence.
PC command sequence zero is rejected. Telemetry uses an independent frame sequence and a `uint32` payload sample
counter for loss detection.

See [`Protocol.md`](docs/Protocol.md) and the byte-exact vectors in
[`Protocol/TestVectors`](Protocol/TestVectors/).

## Implemented command behavior

| PC command | MCU success response | Key rule |
|---|---|---|
| `PING` | `ACK` | Empty payload; IDLE or STREAMING |
| `GET_DEVICE_INFO` | `DEVICE_INFO` | Empty payload; IDLE or STREAMING |
| `SET_STREAM_CONFIG` | `ACK` | `interval_us` 1,000–60,000; IDLE only |
| `START_STREAM` | `ACK` | IDLE only; resets stream counters |
| `STOP_STREAM` | `ACK` | STREAMING only; stops telemetry |

Rejected decodable commands receive `NACK` with the request ID, result code, and current state. Malformed length
candidates and CRC-invalid frames are discarded and resynchronized without a response, as required by the
contract.

`DEVICE_STATUS` and `ERROR_REPORT` identifiers and payloads are recognized as reserved contract elements, but
this MCU revision does not emit them. The current contract does not yet allocate a request/trigger rule for
`DEVICE_STATUS` or error-code semantics for `ERROR_REPORT`; this implementation does not invent those semantics.

## Coding-rules baseline

Firmware v0.2.1 preserves the `Embedded_C_Coding_Rules.md` v1.0.17 application established at MCU commit
`4b1b701`, referencing framework commit `7a68980ef5faa2e897a3574af121683d65f74638`.

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

## Runtime flow

```text
TIM6 IRQ ──> pending configurable sample ticks ─┐
USART2 IRQ ──> RX/TX ring buffers ──────────────┼─> main-context event processing
UART error ──> event flag ──────────────────────┘
```

Interrupt handlers only move bytes, acknowledge hardware, update bounded counters, and post events. Parsing,
command handling, state transitions, telemetry construction, timeout handling, and sine generation execute in
main context.

## Validation

```bash
python3 Tools/validate_project.py
make host-test
python3 Tools/serial_smoke_test.py --self-test
python3 Tools/test_linker_layout.py
bash Tools/build_with_clang.sh
```

The validator checks the pinned Protocol snapshot hash, Message IDs, frame constants, shared vector encoding and
CRC, legacy-protocol removal, Coding Rules mechanics, target metadata, and linker layout. These checks do not
replace human review, STM32CubeIDE build evidence, or physical-board interoperability testing.

## Documentation

- [`Protocol_Authority_Record.md`](docs/Protocol_Authority_Record.md)
- [`Protocol.md`](docs/Protocol.md)
- [`Architecture.md`](docs/Architecture.md)
- [`Hardware_Setup.md`](docs/Hardware_Setup.md)
- [`Verification_Plan.md`](docs/Verification_Plan.md)
- [`Global_Object_Register.md`](docs/design/Global_Object_Register.md)
- [`Deviation_Records.md`](docs/design/Deviation_Records.md)

## Status

Draft MCU implementation baseline v0.2.1, derived from MCU commit `4b1b701` and aligned in code to the pinned
shared Protocol 0.1.0 contract. Software checks do not replace an STM32CubeIDE clean build, firmware download,
PC/MCU interoperability test, and human lifecycle approval.
