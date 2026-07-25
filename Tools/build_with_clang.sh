#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build-clang"
TARGET="$BUILD_DIR/host-device-control-poc-stm32f446re-fw.elf"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

SOURCES=(
  Core/Src/main.c
  Core/Src/startup_stm32f446xx.c
  Core/Src/syscalls.c
  Platform/Src/platform.c
  App/Src/app.c
  App/Src/app_event.c
  App/Src/sine_generator.c
  Protocol/Src/protocol.c
  Protocol/Src/protocol_crc.c
  Transport/Src/serial_transport.c
)

OBJECTS=()
for source in "${SOURCES[@]}"; do
  object="$BUILD_DIR/${source//\//_}.o"
  clang --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
    -std=c11 -ffreestanding -fno-builtin -fdata-sections -ffunction-sections \
    -Wall -Wextra -Werror -Wshadow -Wundef -Wconversion \
    -ICore/Inc -IPlatform/Inc -IApp/Inc -IProtocol/Inc -ITransport/Inc \
    -c "$ROOT/$source" -o "$object"
  OBJECTS+=("$object")
done

ld.lld -flavor gnu -T "$ROOT/STM32F446RETX_FLASH.ld" \
  --gc-sections "${OBJECTS[@]}" -o "$TARGET"

llvm-objcopy -O binary "$TARGET" "$BUILD_DIR/host-device-control-poc-stm32f446re-fw.bin"
llvm-size "$TARGET" 2>/dev/null || true
printf 'clang ARM build: PASS\n'
