#!/usr/bin/env python3
"""Validate repository structure and high-value Embedded C Coding Rules."""

from __future__ import annotations

from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".c", ".h"}
COPYRIGHT = "// Copyright (c) 2026 Ray Yang. All rights reserved."
FUNCTION_FIELDS = (
    "Function:",
    "Purpose:",
    "Input Parameters:",
    "Output Parameters:",
    "Return Value:",
)
REQUIRED_FILES = (
    ".project",
    ".cproject",
    "README.md",
    "CHANGELOG.md",
    "LICENSE",
    "NOTICE.md",
    "Makefile",
    "STM32F446RETX_FLASH.ld",
    "docs/design/Global_Object_Register.md",
    "docs/design/Deviation_Records.md",
    "docs/compliance/Coding_Rules_Application_Report.md",
    "Core/Src/main.c",
    "Core/Src/startup_stm32f446xx.c",
    "Core/Src/syscalls.c",
    "Platform/Src/platform.c",
    "App/Src/app.c",
    "App/Src/app_event.c",
    "Protocol/Inc/protocol_messages.h",
    "Protocol/Src/protocol.c",
    "Transport/Src/serial_transport.c",
    "Protocol/Spec/Host_Device_Control_PoC_protocol.yaml",
    "Tests/protocol_roundtrip_test.c",
)
PROHIBITED_PATTERNS = {
    r"\bmalloc\s*\(": "dynamic allocation: malloc",
    r"\bcalloc\s*\(": "dynamic allocation: calloc",
    r"\brealloc\s*\(": "dynamic allocation: realloc",
    r"\bfree\s*\(": "dynamic allocation: free",
    r"\bHAL_Delay\s*\(": "blocking HAL delay",
    r"\bHAL_UART_Transmit\s*\(": "blocking HAL UART transmit",
    r"\bosThread": "CMSIS-RTOS reference",
    r"\bxTaskCreate\s*\(": "FreeRTOS task creation",
    r"\bwhile\s*\(\s*1[uUlL]*\s*\)": "prohibited intentional infinite-loop form",
    r"/\*\*": "Doxygen-style block comment",
    r"\(void\)app_send_frame\s*\(": "ignored application transmit result",
}
FUNCTION_SIGNATURE = re.compile(
    r"^[ ]*(?:static[ ]+)?(?:inline[ ]+)?(?:const[ ]+)?"
    r"(?:void|bool|int|uint8_t|uint16_t|uint32_t|int16_t|size_t|uintptr_t|"
    r"[a-z][a-z0-9_]*_t)[ ]+\**([A-Za-z_][A-Za-z0-9_]*)[ ]*\(",
    re.MULTILINE,
)
LOWER_SNAKE = re.compile(r"^_?[a-z][a-z0-9_]*$")
CJK = re.compile(r"[\u3400-\u9fff\uf900-\ufaff]")


def source_files() -> list[Path]:
    return sorted(
        path
        for path in ROOT.rglob("*")
        if path.is_file()
        and path.suffix in SOURCE_SUFFIXES
        and "build" not in path.parts
        and "build-clang" not in path.parts
        and "Debug" not in path.parts
        and "Release" not in path.parts
    )


def expected_prefixes(path: Path) -> tuple[str, ...]:
    relative = path.relative_to(ROOT).as_posix()
    if relative.startswith("App/"):
        if "app_event" in relative:
            return ("app_event_",)
        if "sine_generator" in relative:
            return ("sine_generator_",)
        if relative.endswith("device_state.h"):
            return ()
        return ("app_",)
    if relative.startswith("Platform/"):
        if relative.endswith("stm32f446_minimal.h"):
            return ("stm32_nvic_",)
        return ("platform_",)
    if relative.startswith("Protocol/"):
        return ("protocol_",)
    if relative.startswith("Transport/"):
        return ("serial_transport_",)
    if relative.startswith("Core/"):
        if "startup" in relative:
            return ("startup_",)
        if "syscalls" in relative:
            return ("_close", "_lseek", "_read", "_write")
        return ("main", "main_")
    if relative.startswith("Tests/"):
        return ("main", "host_test_")
    return ()


def validate_file_header(path: Path, text: str, errors: list[str]) -> None:
    relative = path.relative_to(ROOT)
    lines = text.splitlines()
    if not lines or lines[0] != COPYRIGHT:
        errors.append(f"{relative}: missing approved copyright as first meaningful content")
    if text.count(COPYRIGHT) != 1:
        errors.append(f"{relative}: expected exactly one approved copyright line")
    if f"//     {path.name}" not in text[:1000]:
        errors.append(f"{relative}: File Header name does not match actual file name")
    required_boundary = "// Responsibilities:" if path.suffix == ".c" else "// Public Contract:"
    for token in ("// File:", "// Purpose:", required_boundary):
        if token not in text[:1400]:
            errors.append(f"{relative}: File Header missing {token}")
    for prohibited in ("// Author:", "// Authors:", "// Maintainer:", "// Revision History:"):
        if prohibited in text[:1400]:
            errors.append(f"{relative}: prohibited File Header field {prohibited}")


def validate_function_headers(path: Path, text: str, errors: list[str]) -> None:
    relative = path.relative_to(ROOT)
    for match in re.finditer(r"/\*.*?\*/", text, re.DOTALL):
        block = match.group(0)
        if not all(field in block for field in FUNCTION_FIELDS):
            errors.append(f"{relative}: general block comment or incomplete Function Header")
            continue
        name_match = re.search(r"\*     ([A-Za-z_][A-Za-z0-9_]*)\s*\n", block)
        if name_match is None:
            errors.append(f"{relative}: Function Header does not identify a function")
            continue
        after = text[match.end():].lstrip()
        if re.match(rf"(?:static\s+)?(?:inline\s+)?(?:const\s+)?[\w\s\*]+\b{name_match.group(1)}\s*\(", after) is None:
            errors.append(f"{relative}: Function Header for {name_match.group(1)} is not immediately followed by that function")

    prefixes = expected_prefixes(path)
    for signature in FUNCTION_SIGNATURE.finditer(text):
        name = signature.group(1)
        before = text[:signature.start()].rstrip()
        if not before.endswith("*/"):
            errors.append(f"{relative}: {name} lacks an immediately preceding Function Header")
        else:
            block_start = before.rfind("/*")
            block = before[block_start:]
            if f"*     {name}\n" not in block:
                errors.append(f"{relative}: Function Header name does not match {name}")
        if LOWER_SNAKE.fullmatch(name) is None:
            errors.append(f"{relative}: function is not lower snake case: {name}")
        if prefixes and not any(name == prefix or name.startswith(prefix) for prefix in prefixes):
            errors.append(f"{relative}: function lacks approved module prefix: {name}")


def parse_c_message_ids(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"^#define\s+PROTOCOL_MESSAGE_([A-Z0-9_]+)\s+\(0x([0-9A-Fa-f]+)u\)\s*$",
        re.MULTILINE,
    )
    return {name: int(value, 16) for name, value in pattern.findall(text)}


def parse_yaml_message_ids(path: Path) -> dict[str, int]:
    result: dict[str, int] = {}
    in_messages = False
    current_name: str | None = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        if raw_line == "messages:":
            in_messages = True
            continue
        if in_messages and raw_line and not raw_line.startswith(" "):
            break
        if not in_messages:
            continue
        name_match = re.match(r"^  - name:\s+([A-Z0-9_]+)\s*$", raw_line)
        if name_match:
            current_name = name_match.group(1)
            continue
        id_match = re.match(r"^    id:\s+0x([0-9A-Fa-f]+)\s*$", raw_line)
        if id_match and current_name is not None:
            result[current_name] = int(id_match.group(1), 16)
            current_name = None
    return result


def main() -> int:
    errors: list[str] = []
    for relative_path in REQUIRED_FILES:
        if not (ROOT / relative_path).is_file():
            errors.append(f"missing required file: {relative_path}")

    files = source_files()
    for path in files:
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(ROOT)
        validate_file_header(path, text, errors)
        validate_function_headers(path, text, errors)
        if "\t" in text:
            errors.append(f"{relative}: tab character found")
        if CJK.search(text):
            errors.append(f"{relative}: unexpected CJK narrative content")
        for line_number, line in enumerate(text.splitlines(), 1):
            if len(line) > 120:
                errors.append(f"{relative}:{line_number}: line exceeds 120 characters")
        for pattern, description in PROHIBITED_PATTERNS.items():
            if re.search(pattern, text):
                errors.append(f"{relative}: prohibited {description}")

    if list(ROOT.rglob("*.ioc")):
        errors.append("unexpected .ioc file in register-level baseline")

    for xml_name in (".project", ".cproject"):
        try:
            ET.parse(ROOT / xml_name)
        except (FileNotFoundError, ET.ParseError) as exc:
            errors.append(f"{xml_name}: invalid or missing XML: {exc}")

    c_ids = parse_c_message_ids(ROOT / "Protocol/Inc/protocol_messages.h")
    yaml_ids = parse_yaml_message_ids(ROOT / "Protocol/Spec/Host_Device_Control_PoC_protocol.yaml")
    if c_ids != yaml_ids:
        errors.append("protocol Message IDs differ between C and YAML")
    if len(set(c_ids.values())) != len(c_ids):
        errors.append("protocol header contains duplicate Message IDs")

    startup_text = (ROOT / "Core/Src/startup_stm32f446xx.c").read_text(encoding="utf-8")
    gor_text = (ROOT / "docs/design/Global_Object_Register.md").read_text(encoding="utf-8")
    if "// Global Object Record: GOR-STARTUP-001." not in startup_text:
        errors.append("startup vector table lacks its Global Object Record reference")
    if "GOR-STARTUP-001" not in gor_text or "g_startup_vector_table" not in gor_text:
        errors.append("Global Object Register lacks GOR-STARTUP-001")

    linker_text = (ROOT / "STM32F446RETX_FLASH.ld").read_text(encoding="utf-8")
    for token in ("flash PT_LOAD FLAGS(5)", "data  PT_LOAD FLAGS(6)", ".bss (NOLOAD)", "> RAM :NONE"):
        if token not in linker_text:
            errors.append(f"linker script missing required token: {token}")

    cproject_text = (ROOT / ".cproject").read_text(encoding="utf-8")
    for token in ("STM32F446RETx", "NUCLEO-F446RE", "fpv4-sp-d16", "floatabi.value.hard"):
        if token not in cproject_text:
            errors.append(f".cproject missing target metadata: {token}")

    if errors:
        print("validation: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    print(f"validation: PASS ({len(files)} Product-owned C/header files, {len(c_ids)} messages)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
