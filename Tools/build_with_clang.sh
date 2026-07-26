#!/usr/bin/env bash
# Copyright (c) 2026 Ray Yang. All rights reserved.

set -euo pipefail

for tool in clang ld.lld; do
    if ! command -v "${tool}" >/dev/null; then
        echo "missing tool: ${tool}" >&2
        exit 127
    fi
done

if command -v llvm-size >/dev/null; then
    size_tool="llvm-size"
elif command -v size >/dev/null; then
    size_tool="size"
else
    echo "missing tool: llvm-size or size" >&2
    exit 127
fi

serial_baud="${SERIAL_BAUD:-460800}"
case "${serial_baud}" in
    1200|2400|4800|9600|19200|38400|57600|115200|230400|460800|921600)
        ;;
    *)
        echo "unsupported SERIAL_BAUD: ${serial_baud}" >&2
        exit 2
        ;;
esac

output_directory="build/clang-${serial_baud}"
rm -rf "${output_directory}"
mkdir -p "${output_directory}"

include_flags=(
    -IApp/Inc
    -IProtocol/Inc
    -ITransport/Inc
    -IPlatform/Inc
)

compiler_flags=(
    --target=arm-none-eabi
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    -std=c11
    -ffreestanding
    -fno-builtin
    -fdata-sections
    -ffunction-sections
    -Wall
    -Wextra
    -Wshadow
    -Wundef
    -Wconversion
    -Wdouble-promotion
    -Wformat=2
    -Werror
    "-DSERIAL_TRANSPORT_BAUD_RATE=${serial_baud}u"
)

sources=(
    App/Src/app.c
    App/Src/app_event.c
    App/Src/waveform_generator.c
    Core/Src/main.c
    Core/Src/startup_stm32f446xx.c
    Core/Src/syscalls.c
    Platform/Src/platform_stm32f446re.c
    Protocol/Src/protocol.c
    Transport/Src/serial_transport.c
)

objects=()
for source_path in "${sources[@]}"; do
    object_path="${output_directory}/$(basename "${source_path%.c}").o"
    clang \
        "${compiler_flags[@]}" \
        "${include_flags[@]}" \
        -c "${source_path}" \
        -o "${object_path}"
    objects+=("${object_path}")
done

ld.lld \
    -T STM32F446RETX_FLASH.ld \
    --gc-sections \
    -Map="${output_directory}/firmware.map" \
    -o "${output_directory}/firmware.elf" \
    "${objects[@]}"

printf 'SERIAL_BAUD=%s\n' "${serial_baud}"
"${size_tool}" "${output_directory}/firmware.elf"
