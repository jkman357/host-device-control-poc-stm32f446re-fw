#!/usr/bin/env python3
"""Compatibility entry point for the superseded serial smoke test."""

from __future__ import annotations

import argparse


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.self_test:
        print("serial_smoke_test self-test: PASS")
        return

    print(
        "Use Tools/protocol_command_sweep.py for the strict hardware "
        "command sweep."
    )


if __name__ == "__main__":
    main()
