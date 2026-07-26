#!/usr/bin/env bash
# Copyright (c) 2026 Ray Yang. All rights reserved.

set -euo pipefail

baud_rates=(
    1200
    2400
    4800
    9600
    19200
    38400
    57600
    115200
    230400
    460800
    921600
)

for baud_rate in "${baud_rates[@]}"; do
    SERIAL_BAUD="${baud_rate}" bash Tools/build_with_clang.sh
done

echo "all baud profile builds: PASS"
