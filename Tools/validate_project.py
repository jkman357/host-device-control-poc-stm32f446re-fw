#!/usr/bin/env python3
"""Mechanical validation for the STM32F446RE PoC repository."""

from __future__ import annotations

import ast
import hashlib
import os
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import List, Optional

DEFAULT_ROOT = Path(__file__).resolve().parents[1]
ROOT = Path(os.environ.get("PROJECT_ROOT", DEFAULT_ROOT)).resolve()
ERRORS: List[str] = []


def read_text(relative_path: str) -> str:
    path = ROOT / relative_path
    if not path.is_file():
        ERRORS.append(f"missing file: {relative_path}")
        return ""
    return path.read_text(encoding="utf-8")


def require_token(relative_path: str, token: str) -> str:
    text = read_text(relative_path)
    if token not in text:
        ERRORS.append(f"{relative_path}: missing {token!r}")
    return text


def function_body(text: str, function_name: str) -> Optional[str]:
    match = re.search(
        rf"\b{re.escape(function_name)}\s*\([^;]*?\)\s*\{{",
        text,
        re.DOTALL,
    )
    if match is None:
        return None

    start = match.end() - 1
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]
    return None


def parse_define_u32(text: str, name: str) -> Optional[int]:
    match = re.search(
        rf"^\s*#define\s+{re.escape(name)}\s+\((0x[0-9A-Fa-f]+|[0-9]+)u\)\s*$",
        text,
        re.MULTILINE,
    )
    if match is None:
        ERRORS.append(f"unable to parse {name}")
        return None
    return int(match.group(1), 0)


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for data_byte in data:
        crc ^= data_byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def validate_required_files() -> None:
    required = [
        ".project",
        ".cproject",
        "README.md",
        "CHANGELOG.md",
        "LICENSE",
        "NOTICE.md",
        "Makefile",
        "STM32F446RETX_FLASH.ld",
        "App/Src/app.c",
        "App/Src/app_event.c",
        "Core/Src/main.c",
        "Core/Src/startup_stm32f446xx.c",
        "Core/Src/syscalls.c",
        "Platform/Src/platform_stm32f446re.c",
        "Protocol/Src/protocol.c",
        "Transport/Src/serial_transport.c",
        "Tests/app_event_ordering_test.c",
        "Tests/app_command_ordering_test.c",
        "Tests/protocol_roundtrip_test.c",
        "Tools/protocol_command_sweep.py",
    ]
    for relative_path in required:
        read_text(relative_path)


def validate_project_metadata() -> None:
    for relative_path in [".project", ".cproject"]:
        path = ROOT / relative_path
        if not path.is_file():
            continue
        try:
            ET.parse(path)
        except ET.ParseError as exc:
            ERRORS.append(f"{relative_path}: invalid XML: {exc}")

    project = read_text(".project")
    cproject = read_text(".cproject")

    for token in [
        "host-device-control-poc-stm32f446re-fw",
        "STM32F446RETx",
        "NUCLEO-F446RE",
        "fpv4-sp-d16",
        "floatabi.value.hard",
        "STM32F446RETX_FLASH.ld",
    ]:
        if token not in (project + cproject):
            ERRORS.append(f"STM32CubeIDE metadata missing: {token}")

    for source_directory in ["App", "Core", "Platform", "Protocol", "Transport"]:
        token = f'kind="sourcePath" name="{source_directory}"'
        if token not in cproject:
            ERRORS.append(f".cproject source entry missing: {source_directory}")


def validate_protocol_snapshot() -> None:
    snapshot_path = ROOT / "Protocol/Spec/Host_Device_Control_PoC_protocol.yaml"
    record = read_text("docs/Protocol_Authority_Record.md")
    if not snapshot_path.is_file():
        ERRORS.append("missing protocol snapshot")
        return

    digest = hashlib.sha256(snapshot_path.read_bytes()).hexdigest()
    match = re.search(r"Local snapshot SHA-256:\s*`([0-9a-f]{64})`", record)
    if match is None:
        ERRORS.append("Protocol_Authority_Record.md: snapshot hash not found")
    elif match.group(1) != digest:
        ERRORS.append("protocol snapshot SHA-256 does not match authority record")


def validate_protocol_identifiers() -> None:
    messages = read_text("Protocol/Inc/protocol_messages.h")
    expected = {
        "PROTOCOL_MESSAGE_PING": 0x01,
        "PROTOCOL_MESSAGE_GET_DEVICE_INFO": 0x02,
        "PROTOCOL_MESSAGE_SET_STREAM_CONFIG": 0x03,
        "PROTOCOL_MESSAGE_START_STREAM": 0x04,
        "PROTOCOL_MESSAGE_STOP_STREAM": 0x05,
        "PROTOCOL_MESSAGE_ACK": 0x80,
        "PROTOCOL_MESSAGE_NACK": 0x81,
        "PROTOCOL_MESSAGE_DEVICE_INFO": 0x82,
        "PROTOCOL_MESSAGE_DEVICE_STATUS": 0x83,
        "PROTOCOL_MESSAGE_TELEMETRY_SAMPLE": 0x90,
        "PROTOCOL_MESSAGE_ERROR_REPORT": 0x91,
    }
    values = []
    for name, expected_value in expected.items():
        value = parse_define_u32(messages, name)
        if value is not None:
            values.append(value)
            if value != expected_value:
                ERRORS.append(
                    f"{name}: expected 0x{expected_value:02X}, got 0x{value:02X}"
                )
    if len(values) != len(set(values)):
        ERRORS.append("protocol message identifiers are not unique")


def validate_test_vectors() -> None:
    vector_directory = ROOT / "Protocol/TestVectors"
    vector_paths = sorted(vector_directory.glob("*.hex"))
    if not vector_paths:
        ERRORS.append("no Protocol/TestVectors/*.hex files found")
        return

    for path in vector_paths:
        try:
            raw = bytes.fromhex(path.read_text(encoding="utf-8"))
        except ValueError:
            ERRORS.append(f"{path.relative_to(ROOT)}: invalid hex")
            continue

        if len(raw) < 10:
            ERRORS.append(f"{path.relative_to(ROOT)}: frame is too short")
            continue
        if raw[:2] != b"\xA5\x5A":
            ERRORS.append(f"{path.relative_to(ROOT)}: invalid SOF")
            continue
        payload_length = int.from_bytes(raw[6:8], "little")
        if len(raw) != 10 + payload_length:
            ERRORS.append(f"{path.relative_to(ROOT)}: payload length mismatch")
            continue
        received_crc = int.from_bytes(raw[-2:], "little")
        calculated_crc = crc16_ccitt_false(raw[2:-2])
        if received_crc != calculated_crc:
            ERRORS.append(f"{path.relative_to(ROOT)}: CRC mismatch")


def validate_fpu_startup() -> None:
    startup = require_token(
        "Core/Src/startup_stm32f446xx.c", "startup_enable_fpu();"
    )
    required_tokens = [
        "0xE000ED88u",
        "3u << 20u",
        "3u << 22u",
        '"DSB"',
        '"ISB"',
    ]
    for token in required_tokens:
        if token not in startup:
            ERRORS.append(f"startup: missing exact FPU control {token}")

    reset_body = function_body(startup, "startup_reset_handler")
    if reset_body is None:
        ERRORS.append("startup: reset handler body not found")
        return

    ordered_tokens = [
        "startup_enable_fpu();",
        "while (destination < &_edata)",
        "for (destination = &_sbss",
        "(void)main();",
    ]
    positions = [reset_body.find(token) for token in ordered_tokens]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        ERRORS.append("startup: FPU/data/bss/main order is invalid")


def validate_bandwidth() -> None:
    transport_header = read_text("Transport/Inc/serial_transport.h")
    transport_source = read_text("Transport/Src/serial_transport.c")
    app_source = read_text("App/Src/app.c")

    baud = parse_define_u32(transport_header, "SERIAL_TRANSPORT_BAUD_RATE")
    if baud != 460800:
        ERRORS.append("SERIAL_TRANSPORT_BAUD_RATE must be 460800")
    brr_match = re.search(
        r"#define\s+USART2_BRR_16MHZ_460800\s+\((0x[0-9A-Fa-f]+|[0-9]+)u\)",
        transport_source,
    )
    if brr_match is None:
        ERRORS.append("USART2 BRR constant for 460800 is missing")
    elif int(brr_match.group(1), 0) != 0x23:
        ERRORS.append("USART2 BRR must be 0x0023 for 16 MHz / 460800")
    if "115200" in transport_header or "115200" in transport_source:
        ERRORS.append("legacy 115200 transport setting remains")
    if "_Static_assert(UART_CAPACITY_BITS_PER_SECOND >= UART_REQUIRED_WITH_RESERVE" not in app_source:
        ERRORS.append("compile-time UART bandwidth assertion is missing")

    telemetry_required = 1000 * 24 * 10
    required_with_reserve = telemetry_required * 120 // 100
    if baud is not None and baud < required_with_reserve:
        ERRORS.append("configured baud cannot carry 1 kHz telemetry with reserve")


def validate_event_ordering() -> None:
    event_header = read_text("App/Inc/app_event.h")
    event_source = read_text("App/Src/app_event.c")
    platform_source = read_text("Platform/Src/platform_stm32f446re.c")
    transport_source = read_text("Transport/Src/serial_transport.c")
    queue_test = read_text("Tests/app_event_ordering_test.c")
    command_test = read_text("Tests/app_command_ordering_test.c")

    for token in [
        "APP_EVENT_TYPE_UART_RX_BYTE",
        "APP_EVENT_TYPE_SAMPLE_TICK",
        "app_event_post_rx_byte_from_isr",
    ]:
        if token not in event_header:
            ERRORS.append(f"event queue contract incomplete: {token}")

    if "#define TIM6_DAC_IRQ_PRIORITY (5u)" not in platform_source:
        ERRORS.append("TIM6 priority must be 5")
    if "#define USART2_IRQ_PRIORITY (5u)" not in transport_source:
        ERRORS.append("USART2 priority must match TIM6 to serialize producers")
    if '"WFI"' not in event_source or "if (s_tail == s_head)" not in event_source:
        ERRORS.append("event wait is not race-protected")
    for token in [
        "event.type == APP_EVENT_TYPE_SAMPLE_TICK",
        "event.type == APP_EVENT_TYPE_UART_RX_BYTE",
        "app_event_get_overflow_count() == 1u",
    ]:
        if token not in queue_test:
            ERRORS.append(f"queue ordering test is missing: {token}")
    for token in [
        "push_tick();",
        "start_length - 1u",
        "stop_length - 1u",
        "s_sample_counters[2] == 2u",
    ]:
        if token not in command_test:
            ERRORS.append(f"app command ordering test incomplete: {token}")


def validate_command_sweep() -> None:
    sweep_path = ROOT / "Tools/protocol_command_sweep.py"
    sweep = read_text("Tools/protocol_command_sweep.py")
    try:
        ast.parse(sweep, filename=str(sweep_path))
    except SyntaxError as exc:
        ERRORS.append(f"protocol_command_sweep.py syntax error: {exc}")

    required_tokens = [
        "pending_frames",
        "first_sample_counter == 1",
        "sample_gaps == 0",
        "nonzero_status_frames == 0",
        "parser.crc_error_count == 0",
        "parser.format_error_count == 0",
        "strict sweep criteria failed",
        "Protocol command sweep: STATISTICS ONLY",
        "payload[0] == request_id",
        "len(pending_frames) == 2",
    ]
    for token in required_tokens:
        if token not in sweep:
            ERRORS.append(f"command sweep strictness missing: {token}")



def validate_newlib_syscalls() -> None:
    syscalls = read_text("Core/Src/syscalls.c")
    required_tokens = [
        "int _close(int file_descriptor)",
        "int _lseek(int file_descriptor, int offset, int origin)",
        "int _read(int file_descriptor, char *buffer, int length)",
        "int _write(int file_descriptor, char *buffer, int length)",
        "return -1;",
    ]
    for token in required_tokens:
        if token not in syscalls:
            ERRORS.append(f"newlib syscall boundary incomplete: {token}")

    if "printf(" in syscalls or "malloc(" in syscalls:
        ERRORS.append("syscall stubs must not depend on hosted I/O or allocation")

def validate_no_cubemx_ioc() -> None:
    ioc_files = list(ROOT.glob("*.ioc"))
    if ioc_files:
        ERRORS.append("this register-level baseline must not contain a CubeMX .ioc")
    readme = read_text("README.md")
    if "intentionally contains no `.ioc`" not in readme:
        ERRORS.append("README does not document the no-.ioc boundary")


def main() -> int:
    validate_required_files()
    validate_project_metadata()
    validate_protocol_snapshot()
    validate_protocol_identifiers()
    validate_test_vectors()
    validate_fpu_startup()
    validate_bandwidth()
    validate_event_ordering()
    validate_command_sweep()
    validate_newlib_syscalls()
    validate_no_cubemx_ioc()

    if ERRORS:
        print("Validation FAILED")
        for error in ERRORS:
            print(f" - {error}")
        return 1

    print("Validation PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
