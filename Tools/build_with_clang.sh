#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build-clang"
TARGET="$BUILD_DIR/host-device-control-poc-stm32f446re-fw.elf"
BINARY="$BUILD_DIR/host-device-control-poc-stm32f446re-fw.bin"

CLANG="${CLANG:-clang}"
LLD="${LLD:-ld.lld}"
LLVM_OBJCOPY="${LLVM_OBJCOPY:-llvm-objcopy}"
LLVM_SIZE="${LLVM_SIZE:-llvm-size}"

require_tool() {
  local tool="$1"

  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'required build tool not found: %s\n' "$tool" >&2
    return 1
  fi
}

require_tool "$CLANG"
require_tool "$LLD"
require_tool "$LLVM_OBJCOPY"

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
  "$CLANG" --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
    -std=c11 -ffreestanding -fno-builtin -fdata-sections -ffunction-sections \
    -Wall -Wextra -Werror -Wshadow -Wundef -Wconversion \
    -ICore/Inc -IPlatform/Inc -IApp/Inc -IProtocol/Inc -ITransport/Inc \
    -c "$ROOT/$source" -o "$object"
  OBJECTS+=("$object")
done

"$LLD" -flavor gnu -T "$ROOT/STM32F446RETX_FLASH.ld" \
  --gc-sections "${OBJECTS[@]}" -o "$TARGET"

"$LLVM_OBJCOPY" -O binary "$TARGET" "$BINARY"

if command -v "$LLVM_SIZE" >/dev/null 2>&1; then
  "$LLVM_SIZE" "$TARGET"
else
  printf 'optional build tool not found; size report skipped: %s\n' "$LLVM_SIZE"
fi

printf 'clang ARM build: PASS\n'
