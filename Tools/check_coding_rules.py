#!/usr/bin/env python3
"""Check high-value Embedded C coding-rule requirements for Product-owned sources."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOTS = ("App", "Core", "Platform", "Protocol", "Transport", "Tests")
REQUIRED_FUNCTION_FIELDS = (
    "Function:",
    "Purpose:",
    "Input Parameters:",
    "Output Parameters:",
    "Return Value:",
)
FUNCTION_PATTERN = re.compile(
    r"(?m)^(?:extern\s+)?(?:static\s+)?(?:inline\s+)?(?:const\s+)?"
    r"(?:void|bool|float|int|size_t|uint8_t|uint16_t|uint32_t|uint64_t|"
    r"int8_t|int16_t|int32_t|int64_t|[A-Za-z_][A-Za-z0-9_]*_t)"
    r"\s+\**\s*(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\("
)
BLOCK_COMMENT_PATTERN = re.compile(r"/\*.*?\*/", re.DOTALL)
CJK_PATTERN = re.compile(r"[\u3400-\u9fff]")
DYNAMIC_MEMORY_PATTERN = re.compile(r"\b(?:malloc|calloc|realloc|free)\s*\(")
MACRO_PATTERN = re.compile(r"(?m)^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)")
STATIC_FUNCTION_PATTERN = re.compile(
    r"(?m)^static\s+(?:inline\s+)?(?:const\s+)?"
    r"(?:void|bool|float|int|size_t|uint8_t|uint16_t|uint32_t|uint64_t|"
    r"int8_t|int16_t|int32_t|int64_t|[A-Za-z_][A-Za-z0-9_]*_t)"
    r"\s+\**\s*([A-Za-z_][A-Za-z0-9_]*)\s*\("
)
STATIC_PREFIXES = {
    "App/Src/app.c": "app_",
    "App/Src/app_event.c": "app_event_",
    "App/Src/waveform_generator.c": "waveform_generator_",
    "Core/Src/startup_stm32f446xx.c": "startup_",
    "Protocol/Src/protocol.c": "protocol_",
    "Transport/Src/serial_transport.c": "serial_transport_",
    "Tests/app_baud_policy_test.c": "app_baud_test_",
    "Tests/app_command_ordering_test.c": "app_command_test_",
    "Tests/app_waveform_rotation_test.c": "app_waveform_test_",
    "Tests/serial_baud_profile_test.c": "serial_baud_test_",
    "Tests/waveform_generator_test.c": "waveform_test_",
}
INVALID_INFINITE_LOOP_PATTERN = re.compile(
    r"\b(?:while\s*\(\s*1[uUlL]*\s*\)|for\s*\(\s*;\s*;\s*\)\s*;)"
)


def source_files() -> Iterable[Path]:
    for source_root in SOURCE_ROOTS:
        for path in sorted((ROOT / source_root).rglob("*")):
            if path.suffix in {".c", ".h"}:
                yield path


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def validate_file_header(path: Path, text: str, errors: list[str]) -> None:
    lines = text.splitlines()
    prefix = "\n".join(lines[:24])
    relative_path = path.relative_to(ROOT)

    required_tokens = (
        "// Copyright (c) 2026 Ray Yang. All rights reserved.",
        "// File:",
        f"//     {path.name}",
        "// Purpose:",
    )
    for token in required_tokens:
        if token not in prefix:
            errors.append(f"{relative_path}: incomplete File Header: missing {token!r}")

    responsibility_field = "// Responsibilities:" if path.suffix == ".c" else "// Public Contract:"
    if responsibility_field not in prefix:
        errors.append(
            f"{relative_path}: incomplete File Header: missing {responsibility_field!r}"
        )

    if not text.startswith("// Copyright"):
        errors.append(f"{relative_path}: File Header is not the first content")


def function_parameter_names(text: str, opening_parenthesis: int) -> list[str]:
    depth = 0
    closing_parenthesis = opening_parenthesis
    for index in range(opening_parenthesis, len(text)):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                closing_parenthesis = index
                break

    parameters = text[opening_parenthesis + 1 : closing_parenthesis].strip()
    if parameters in {"", "void"}:
        return []

    names: list[str] = []
    for parameter in parameters.split(","):
        parameter = parameter.strip()
        name_match = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^]]*\])?$", parameter)
        if name_match is not None:
            names.append(name_match.group(1))
    return names


def validate_function_headers(path: Path, text: str, errors: list[str]) -> None:
    relative_path = path.relative_to(ROOT)

    for match in FUNCTION_PATTERN.finditer(text):
        function_name = match.group("name")
        source_line = text[text.rfind("\n", 0, match.start()) + 1 : text.find("\n", match.start())]
        if "(*" in source_line or source_line.lstrip().startswith("typedef"):
            continue

        preceding_text = text[: match.start()].rstrip()
        if not preceding_text.endswith("*/"):
            errors.append(
                f"{relative_path}:{line_number(text, match.start())}: "
                f"{function_name}: missing immediate Function Header"
            )
            continue

        block_start = preceding_text.rfind("/*")
        block = preceding_text[block_start:]
        for field in REQUIRED_FUNCTION_FIELDS:
            if field not in block:
                errors.append(f"{relative_path}: {function_name}: missing {field}")

        name_pattern = re.compile(rf"\*\s+{re.escape(function_name)}\s*$", re.MULTILINE)
        if name_pattern.search(block) is None:
            errors.append(
                f"{relative_path}: {function_name}: Function Header name does not match"
            )

        opening_parenthesis = text.find("(", match.start())
        for parameter_name in function_parameter_names(text, opening_parenthesis):
            parameter_pattern = re.compile(
                rf"\*\s+{re.escape(parameter_name)}\s*:", re.MULTILINE
            )
            if parameter_pattern.search(block) is None:
                errors.append(
                    f"{relative_path}: {function_name}: parameter "
                    f"{parameter_name!r} is missing from the Function Header"
                )


def validate_general_rules(path: Path, text: str, errors: list[str]) -> None:
    relative_path = path.relative_to(ROOT)

    for number, line in enumerate(text.splitlines(), start=1):
        if "\t" in line:
            errors.append(f"{relative_path}:{number}: tab character is prohibited")
        if len(line) > 120:
            errors.append(
                f"{relative_path}:{number}: line length {len(line)} exceeds 120"
            )
        if CJK_PATTERN.search(line):
            errors.append(f"{relative_path}:{number}: Product-code comments shall be English")

    for match in BLOCK_COMMENT_PATTERN.finditer(text):
        if "Function:" not in match.group(0):
            errors.append(
                f"{relative_path}:{line_number(text, match.start())}: "
                "general block comment is prohibited; use //"
            )

    if DYNAMIC_MEMORY_PATTERN.search(text):
        errors.append(f"{relative_path}: dynamic-memory API is prohibited")

    if INVALID_INFINITE_LOOP_PATTERN.search(text):
        errors.append(
            f"{relative_path}: use for (;;) or while (true) for intentional infinite loops"
        )


    for macro_match in MACRO_PATTERN.finditer(text):
        macro_name = macro_match.group(1)
        if macro_name.upper() != macro_name:
            errors.append(
                f"{relative_path}:{line_number(text, macro_match.start())}: "
                f"macro {macro_name!r} shall use uppercase snake case"
            )

    expected_prefix = STATIC_PREFIXES.get(relative_path.as_posix())
    if expected_prefix is not None:
        for static_match in STATIC_FUNCTION_PATTERN.finditer(text):
            function_name = static_match.group(1)
            if not function_name.startswith(expected_prefix):
                errors.append(
                    f"{relative_path}:{line_number(text, static_match.start())}: "
                    f"file-static function {function_name!r} lacks prefix {expected_prefix!r}"
                )

    if relative_path.as_posix() == "Protocol/Src/protocol.c" and re.search(r"(?m)^\s*union\b", text):
        errors.append(f"{relative_path}: Protocol serialization shall not use union type-punning")

    if path.parent.name == "Tests" and "int main(void)" in text:
        if "return EXIT_SUCCESS;" not in text:
            errors.append(f"{relative_path}: Host-test main shall return EXIT_SUCCESS")


def main() -> int:
    errors: list[str] = []
    paths = list(source_files())

    if not paths:
        print("Coding-rule check FAILED: no Product-owned C sources found")
        return 1

    for path in paths:
        text = path.read_text(encoding="utf-8")
        validate_file_header(path, text, errors)
        validate_function_headers(path, text, errors)
        validate_general_rules(path, text, errors)

    if errors:
        print("Coding-rule check FAILED")
        for error in errors:
            print(f" - {error}")
        return 1

    print(f"Coding-rule check PASS ({len(paths)} Product-owned .c/.h files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
