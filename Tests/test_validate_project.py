#!/usr/bin/env python3
"""Regression tests for Tools/validate_project.py."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Callable

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR = ROOT / "Tools/validate_project.py"


def run_validator(project_root: Path) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    environment["PROJECT_ROOT"] = str(project_root)
    return subprocess.run(
        [sys.executable, str(VALIDATOR)],
        check=False,
        capture_output=True,
        text=True,
        env=environment,
    )


def copy_project(destination: Path) -> None:
    shutil.copytree(
        ROOT,
        destination,
        ignore=shutil.ignore_patterns("build", "__pycache__", "*.pyc"),
    )


def mutate_file(project_root: Path, relative_path: str, old: str, new: str) -> None:
    path = project_root / relative_path
    text = path.read_text(encoding="utf-8")
    if old not in text:
        raise AssertionError(f"mutation token not found: {relative_path}: {old}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def expect_rejected(mutator: Callable[[Path], None], expected_text: str) -> None:
    with tempfile.TemporaryDirectory() as temporary_directory:
        project_root = Path(temporary_directory) / "project"
        copy_project(project_root)
        mutator(project_root)
        result = run_validator(project_root)
        assert result.returncode != 0, result.stdout + result.stderr
        assert expected_text in result.stdout, result.stdout + result.stderr


def main() -> None:
    baseline = run_validator(ROOT)
    assert baseline.returncode == 0, baseline.stdout + baseline.stderr

    expect_rejected(
        lambda project: mutate_file(
            project,
            "Core/Src/startup_stm32f446xx.c",
            '"DSB"',
            '"NOP"',
        ),
        "missing exact FPU control",
    )
    expect_rejected(
        lambda project: mutate_file(
            project,
            "Transport/Inc/serial_baud.h",
            "SERIAL_BAUD_RATE_921600 (921600u)",
            "SERIAL_BAUD_RATE_921600 (921500u)",
        ),
        "shared baud list is missing 921600",
    )
    expect_rejected(
        lambda project: mutate_file(
            project,
            "Transport/Inc/serial_baud.h",
            "SERIAL_BAUD_COMMAND_ONLY_MAX_RATE SERIAL_BAUD_RATE_9600",
            "SERIAL_BAUD_COMMAND_ONLY_MAX_RATE SERIAL_BAUD_RATE_4800",
        ),
        "baud configuration incomplete",
    )

    expect_rejected(
        lambda project: mutate_file(
            project,
            "Tools/protocol_command_sweep.py",
            "sample_gaps == 0",
            "sample_gaps >= 0",
        ),
        "command sweep strictness missing",
    )
    expect_rejected(
        lambda project: mutate_file(
            project,
            "Platform/Src/platform_stm32f446re.c",
            "TIM6_DAC_IRQ_PRIORITY (5u)",
            "TIM6_DAC_IRQ_PRIORITY (6u)",
        ),
        "TIM6 priority must be 5",
    )
    expect_rejected(
        lambda project: mutate_file(
            project,
            "Transport/Src/serial_transport.c",
            "USART2->brr = USART2_BRR_VALUE;",
            "USART2->brr = 0x0023u;",
        ),
        "USART2 baud validation incomplete",
    )
    expect_rejected(
        lambda project: mutate_file(
            project,
            "Tools/build_all_baud_profiles.sh",
            "    921600\n",
            "",
        ),
        "all-profile build is missing 921600",
    )
    expect_rejected(
        lambda project: mutate_file(
            project,
            "App/Src/app.c",
            "interval_us < APP_EFFECTIVE_STREAM_MIN_INTERVAL_US",
            "interval_us < PROTOCOL_STREAM_INTERVAL_MIN_US",
        ),
        "application baud policy incomplete",
    )

    expect_rejected(
        lambda project: mutate_file(
            project,
            "App/Src/app.c",
            "WAVEFORM_SWITCH_INTERVAL_US             (10000000u)",
            "WAVEFORM_SWITCH_INTERVAL_US             (5000000u)",
        ),
        "waveform rotation incomplete",
    )
    expect_rejected(
        lambda project: mutate_file(
            project,
            "App/Src/waveform_generator.c",
            "static float waveform_sine_polynomial(float angle)",
            "static float waveform_linear_approximation(float angle)",
        ),
        "waveform generator incomplete",
    )

    expect_rejected(
        lambda project: mutate_file(
            project,
            "Core/Src/syscalls.c",
            "int _write(int file_descriptor, char *buffer, int length)",
            "int unsupported_write(int file_descriptor, char *buffer, int length)",
        ),
        "newlib syscall boundary incomplete",
    )

    print("validate_project regression tests: PASS")


if __name__ == "__main__":
    main()
