#!/usr/bin/env python3
"""Validate repository structure, boundaries, and protocol ID consistency."""

from __future__ import annotations

from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]

REQUIRED_FILES = [
    ".project",
    ".cproject",
    ".github/workflows/validate.yml",
    "README.md",
    "CHANGELOG.md",
    "LICENSE",
    "NOTICE.md",
    "Makefile",
    "STM32F446RETX_FLASH.ld",
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
    "docs/Architecture.md",
    "docs/Hardware_Setup.md",
    "docs/Protocol.md",
    "docs/Verification_Plan.md",
    "Tests/protocol_roundtrip_test.c",
    "Tools/check_cubeide_project.py",
    "Tools/serial_smoke_test.py",
]

PROHIBITED_PATTERNS = {
    r"\bmalloc\s*\(": "dynamic allocation: malloc",
    r"\bcalloc\s*\(": "dynamic allocation: calloc",
    r"\brealloc\s*\(": "dynamic allocation: realloc",
    r"\bfree\s*\(": "dynamic allocation: free",
    r"\bHAL_Delay\s*\(": "blocking HAL delay",
    r"\bHAL_UART_Transmit\s*\(": "blocking HAL UART transmit",
    r"\bosThread": "CMSIS-RTOS reference",
    r"\bxTaskCreate\s*\(": "FreeRTOS task creation",
}

SOURCE_SUFFIXES = {".c", ".h"}
PROTOCOL_REQUIRED_TOP_LEVEL = {
    "schema_version",
    "document",
    "protocol",
    "wire_format",
    "id_allocation",
    "namespaces",
    "services",
    "enums",
    "errors",
    "messages",
    "compatibility",
    "code_generation",
}


def parse_c_message_ids(path: Path) -> dict[str, int]:
    """Return PROTOCOL_MESSAGE_* constants from the C header."""
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r"^#define\s+PROTOCOL_MESSAGE_([A-Z0-9_]+)\s+\(0x([0-9A-Fa-f]+)u\)\s*$",
        re.MULTILINE,
    )
    return {name: int(value, 16) for name, value in pattern.findall(text)}


def parse_yaml_message_ids(path: Path) -> dict[str, int]:
    """Read message name/id pairs from the intentionally simple YAML layout."""
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


def parse_yaml_top_level_keys(path: Path) -> set[str]:
    """Return non-indented mapping keys from the YAML file."""
    keys: set[str] = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*):", line)
        if match:
            keys.add(match.group(1))
    return keys


def main() -> int:
    errors: list[str] = []

    for relative_path in REQUIRED_FILES:
        if not (ROOT / relative_path).is_file():
            errors.append(f"missing required file: {relative_path}")

    source_files = sorted(
        path for path in ROOT.rglob("*")
        if path.is_file()
        and path.suffix in SOURCE_SUFFIXES
        and "build" not in path.parts
        and "build-clang" not in path.parts
    )

    for path in source_files:
        text = path.read_text(encoding="utf-8")
        relative = path.relative_to(ROOT)
        for pattern, description in PROHIBITED_PATTERNS.items():
            if re.search(pattern, text):
                errors.append(f"{relative}: prohibited {description}")

    main_path = ROOT / "Core/Src/main.c"
    if main_path.is_file():
        main_text = main_path.read_text(encoding="utf-8")
        if "for (;;)" not in main_text:
            errors.append("Core/Src/main.c: event-driven superloop not found")
        if "App_ProcessEvents" not in main_text:
            errors.append("Core/Src/main.c: application event dispatch not found")
        if "AppEvent_Wait" not in main_text:
            errors.append("Core/Src/main.c: atomic event wait not found")

    if (ROOT / "poc-446re").exists():
        errors.append("legacy nested poc-446re directory must not exist")

    if list(ROOT.rglob("*.ioc")):
        errors.append("unexpected .ioc file: this register-level baseline is not CubeMX-regenerable")

    casefold_paths: dict[str, Path] = {}
    for path in sorted(ROOT.rglob("*")):
        if "build" in path.parts or "build-clang" in path.parts:
            continue
        relative = path.relative_to(ROOT)
        folded = relative.as_posix().casefold()
        if folded in casefold_paths and casefold_paths[folded] != relative:
            errors.append(
                "case-insensitive path collision: "
                f"{casefold_paths[folded]} and {relative}"
            )
        else:
            casefold_paths[folded] = relative

    project_files = list(ROOT.rglob(".project"))
    if len(project_files) != 1:
        errors.append(f"expected exactly one .project file, found {len(project_files)}")

    for xml_name in (".project", ".cproject"):
        xml_path = ROOT / xml_name
        if xml_path.is_file():
            try:
                ET.parse(xml_path)
            except ET.ParseError as exc:
                errors.append(f"{xml_name}: invalid XML: {exc}")

    protocol_yaml = ROOT / "Protocol/Spec/Host_Device_Control_PoC_protocol.yaml"
    protocol_header = ROOT / "Protocol/Inc/protocol_messages.h"
    if protocol_yaml.is_file():
        missing_keys = PROTOCOL_REQUIRED_TOP_LEVEL - parse_yaml_top_level_keys(protocol_yaml)
        for key in sorted(missing_keys):
            errors.append(f"protocol YAML: missing required top-level key: {key}")

    if protocol_yaml.is_file() and protocol_header.is_file():
        c_ids = parse_c_message_ids(protocol_header)
        yaml_ids = parse_yaml_message_ids(protocol_yaml)
        if not c_ids:
            errors.append("protocol header: no PROTOCOL_MESSAGE_* constants found")
        if not yaml_ids:
            errors.append("protocol YAML: no message IDs found")

        for name, c_value in sorted(c_ids.items()):
            if name not in yaml_ids:
                errors.append(f"protocol YAML: missing message {name}")
            elif yaml_ids[name] != c_value:
                errors.append(
                    f"protocol ID mismatch for {name}: C=0x{c_value:04X}, "
                    f"YAML=0x{yaml_ids[name]:04X}"
                )

        for name in sorted(set(yaml_ids) - set(c_ids)):
            errors.append(f"protocol header: missing message {name}")

        if len(set(c_ids.values())) != len(c_ids):
            errors.append("protocol header: duplicate Message ID")
        if len(set(yaml_ids.values())) != len(yaml_ids):
            errors.append("protocol YAML: duplicate Message ID")

    linker_script_path = ROOT / "STM32F446RETX_FLASH.ld"
    if linker_script_path.is_file():
        linker_text = linker_script_path.read_text(encoding="utf-8")
        required_linker_tokens = (
            "PHDRS",
            "flash PT_LOAD FLAGS(5)",
            "data  PT_LOAD FLAGS(6)",
            "> FLASH :flash",
            "> RAM AT > FLASH :data",
            ".bss (NOLOAD)",
            "._user_heap_stack (NOLOAD)",
        )
        for token in required_linker_tokens:
            if token not in linker_text:
                errors.append(f"linker script: missing segment control: {token}")

        if "ram   PT_LOAD" in linker_text:
            errors.append(
                "linker script: zero-initialized RAM must not use a loadable ram PT_LOAD"
            )

        bss_match = re.search(
            r"\.bss\s*\(NOLOAD\).*?}\s*>\s*RAM\s*:NONE",
            linker_text,
            re.DOTALL,
        )
        if bss_match is None:
            errors.append("linker script: .bss NOLOAD section must use > RAM :NONE")

        stack_match = re.search(
            r"\._user_heap_stack\s*\(NOLOAD\).*?}\s*>\s*RAM\s*:NONE",
            linker_text,
            re.DOTALL,
        )
        if stack_match is None:
            errors.append(
                "linker script: ._user_heap_stack NOLOAD section must use > RAM :NONE"
            )

    cproject_path = ROOT / ".cproject"
    if cproject_path.is_file():
        cproject_text = cproject_path.read_text(encoding="utf-8")
        required_target_tokens = (
            "STM32F446RETx",
            "NUCLEO-F446RE",
            "managedbuild.option.target_cpuid",
            "managedbuild.option.target_coreid",
            "managedbuild.option.defaults",
            "fpv4-sp-d16",
            "floatabi.value.hard",
            "STM32F446RETX_FLASH.ld",
        )
        for token in required_target_tokens:
            if token not in cproject_text:
                errors.append(f".cproject: missing target metadata: {token}")

    for generated_directory in ("Debug", "Release"):
        if (ROOT / generated_directory).exists():
            errors.append(
                f"generated directory must not be packaged: {generated_directory}/"
            )

    if errors:
        print("validation: FAIL")
        for error in errors:
            print(f"- {error}")
        return 1

    c_ids = parse_c_message_ids(protocol_header)
    print(
        "validation: PASS "
        f"({len(source_files)} C/header files, {len(c_ids)} protocol messages checked)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())

