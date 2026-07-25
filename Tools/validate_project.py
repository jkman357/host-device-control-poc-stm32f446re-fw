#!/usr/bin/env python3
"""Validate repository structure and high-value Embedded C Coding Rules."""

from __future__ import annotations

from pathlib import Path
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".c", ".h"}
COPYRIGHT = "// Copyright (c) 2026 Ray Yang. All rights reserved."
PROTOCOL_AUTHORITY_SHA256 = "7ff8db3a1ed669407e0d4cada2a78b212ea3c7bccdf371f232a2689a02e7c56e"

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
    ".github/workflows/validate.yml",
    "Tools/build_with_clang.sh",
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
    "Protocol/TestVectors/protocol-v0.1.0-vectors.json",
    "Protocol/TestVectors/README.md",
    "docs/Protocol_Authority_Record.md",
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



def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            if (crc & 0x8000) != 0:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def encode_protocol_vector(message_id: int, sequence: int, payload: bytes) -> bytes:
    body = bytes((0x01, message_id))
    body += sequence.to_bytes(2, byteorder="little")
    body += len(payload).to_bytes(2, byteorder="little")
    body += payload
    crc = crc16_ccitt_false(body)
    return bytes((0xA5, 0x5A)) + body + crc.to_bytes(2, byteorder="little")


def validate_protocol_authority(errors: list[str]) -> int:
    protocol_path = ROOT / "Protocol/Spec/Host_Device_Control_PoC_protocol.yaml"
    vector_path = ROOT / "Protocol/TestVectors/protocol-v0.1.0-vectors.json"

    protocol_bytes = protocol_path.read_bytes()
    actual_sha = hashlib.sha256(protocol_bytes).hexdigest()
    if actual_sha != PROTOCOL_AUTHORITY_SHA256:
        errors.append(
            "Protocol authority snapshot SHA-256 differs from the pinned system-repository contract"
        )

    protocol_text = protocol_bytes.decode("utf-8")
    required_tokens = (
        "version: 0.1.0",
        "wire_version: 0x01",
        "status: candidate_for_alignment",
        "repository: host-device-control-poc-system",
        "path: protocol/protocol.yaml",
        "rule: specification_precedes_implementation",
        "maximum_payload_size_bytes: 1024",
        "partial_frame: 250",
        "float32: ieee_754_binary32_little_endian",
    )
    for token in required_tokens:
        if token not in protocol_text:
            errors.append(f"Protocol authority snapshot missing required token: {token}")

    try:
        vector_document = json.loads(vector_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as exc:
        errors.append(f"invalid shared vector JSON: {exc}")
        return 0

    if vector_document.get("protocol") != "host-device-control-poc":
        errors.append("shared vectors identify the wrong protocol")
    if vector_document.get("protocol_version") != "0.1.0":
        errors.append("shared vectors identify the wrong protocol version")
    if vector_document.get("wire_version") != 1:
        errors.append("shared vectors identify the wrong wire version")
    if vector_document.get("contract_status") != "candidate_for_alignment":
        errors.append("shared vectors identify the wrong contract status")

    vectors = vector_document.get("vectors")
    if not isinstance(vectors, list):
        errors.append("shared vectors do not contain a vector list")
        return 0

    covered_ids: set[int] = set()
    names: set[str] = set()
    for index, vector in enumerate(vectors):
        if not isinstance(vector, dict):
            errors.append(f"shared vector {index} is not an object")
            continue
        try:
            name = str(vector["name"])
            message_id = int(str(vector["message_id"]), 16)
            sequence = int(vector["sequence"])
            payload = bytes.fromhex(str(vector["payload_hex"]))
            expected_frame = bytes.fromhex(str(vector["frame_hex"]))
        except (KeyError, TypeError, ValueError) as exc:
            errors.append(f"shared vector {index} has invalid fields: {exc}")
            continue

        if name in names:
            errors.append(f"duplicate shared vector name: {name}")
        names.add(name)
        if not (0 <= message_id <= 0xFF):
            errors.append(f"shared vector {name} message ID exceeds uint8")
            continue
        if not (0 <= sequence <= 0xFFFF):
            errors.append(f"shared vector {name} sequence exceeds uint16")
            continue
        if len(payload) > 1024:
            errors.append(f"shared vector {name} payload exceeds 1024 bytes")
            continue

        actual_frame = encode_protocol_vector(message_id, sequence, payload)
        if actual_frame != expected_frame:
            errors.append(f"shared vector {name} does not match framing or CRC rules")
        covered_ids.add(message_id)

    required_coverage = {0x01, 0x03, 0x04, 0x80, 0x90}
    if not required_coverage.issubset(covered_ids):
        errors.append("shared vectors lack command, response, configuration, or telemetry coverage")

    authority_record = (ROOT / "docs/Protocol_Authority_Record.md").read_text(encoding="utf-8")
    for token in (
        "host-device-control-poc-system",
        "protocol/protocol.yaml",
        "0.1.0",
        "candidate_for_alignment",
        PROTOCOL_AUTHORITY_SHA256,
        "4b1b701",
    ):
        if token not in authority_record:
            errors.append(f"Protocol Authority Record missing token: {token}")

    return len(vectors)



def validate_ci_configuration(errors: list[str]) -> None:
    workflow_text = (ROOT / ".github/workflows/validate.yml").read_text(encoding="utf-8")
    workflow_tokens = (
        "runs-on: ubuntu-24.04",
        "uses: actions/checkout@v5",
        "sudo apt-get install --yes --no-install-recommends clang lld llvm",
        "command -v llvm-objcopy",
        "command -v llvm-size",
        "run: bash Tools/build_with_clang.sh",
    )
    for token in workflow_tokens:
        if token not in workflow_text:
            errors.append(f"CI workflow missing required token: {token}")

    build_script_text = (ROOT / "Tools/build_with_clang.sh").read_text(encoding="utf-8")
    script_tokens = (
        'CLANG="${CLANG:-clang}"',
        'LLD="${LLD:-ld.lld}"',
        'LLVM_OBJCOPY="${LLVM_OBJCOPY:-llvm-objcopy}"',
        'require_tool "$LLVM_OBJCOPY"',
        '"$LLVM_OBJCOPY" -O binary',
    )
    for token in script_tokens:
        if token not in build_script_text:
            errors.append(f"Clang build script missing required token: {token}")

    if "actions/checkout@v4" in workflow_text:
        errors.append("CI workflow uses deprecated Node.js 20 checkout action")


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

    platform_text = (ROOT / "Platform/Src/platform.c").read_text(encoding="utf-8")
    if "TIM6_PERIOD_MAX_US" in platform_text:
        errors.append(
            "Platform/Src/platform.c: redundant uint16 TIM6 upper-bound guard reintroduced"
        )

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

    vector_count = validate_protocol_authority(errors)
    validate_ci_configuration(errors)

    protocol_header_text = (ROOT / "Protocol/Inc/protocol.h").read_text(encoding="utf-8")
    for token in (
        "#define PROTOCOL_MAX_PAYLOAD_LENGTH          (1024u)",
        "#define PROTOCOL_FRAME_OVERHEAD_LENGTH       (10u)",
        "#define PROTOCOL_PARTIAL_FRAME_TIMEOUT_US    (250000u)",
        "uint8_t message_id;",
        "uint16_t payload_length;",
    ):
        if token not in protocol_header_text:
            errors.append(f"protocol implementation header missing authoritative token: {token}")

    legacy_markers = {
        "0x2000": "legacy 16-bit telemetry Message ID",
        "PING_RESPONSE": "legacy dedicated PING response",
        "START_STREAM_RESPONSE": "legacy dedicated START response",
        "STOP_STREAM_RESPONSE": "legacy dedicated STOP response",
        "PROTOCOL_FLAG_REQUEST": "legacy flags field",
    }
    legacy_files = (
        ROOT / "README.md",
        ROOT / "docs/Protocol.md",
        ROOT / "docs/Architecture.md",
        ROOT / "docs/Project_Input.md",
        ROOT / "docs/Decision_Log.md",
        ROOT / "docs/Verification_Plan.md",
        ROOT / "Protocol/Inc/protocol_messages.h",
        ROOT / "App/Src/app.c",
    )
    for legacy_file in legacy_files:
        legacy_text = legacy_file.read_text(encoding="utf-8")
        for marker, description in legacy_markers.items():
            if marker in legacy_text:
                errors.append(f"{legacy_file.relative_to(ROOT)}: contains {description}")

    startup_text = (ROOT / "Core/Src/startup_stm32f446xx.c").read_text(encoding="utf-8")
    gor_text = (ROOT / "docs/design/Global_Object_Register.md").read_text(encoding="utf-8")
    for token in (
        "#define SCB_CPACR_ADDRESS",
        "SCB_CPACR_CP10_FULL_ACCESS",
        "SCB_CPACR_CP11_FULL_ACCESS",
        "static void startup_enable_fpu(void)",
        "SCB_CPACR |= SCB_CPACR_FPU_FULL_ACCESS_MASK;",
        "startup_enable_fpu();",
    ):
        if token not in startup_text:
            errors.append(f"startup source missing hard-float enable token: {token}")

    reset_call_index = startup_text.find("startup_enable_fpu();")
    main_call_index = startup_text.find("(void)main();")
    if (reset_call_index < 0) or (main_call_index < 0) or (reset_call_index > main_call_index):
        errors.append("startup must enable the FPU before entering main")

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

    print(
        f"validation: PASS ({len(files)} Product-owned C/header files, "
        f"{len(c_ids)} messages, {vector_count} shared vectors)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
