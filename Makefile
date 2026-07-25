TARGET := host-device-control-poc-stm32f446re-fw
BUILD_DIR := build

PREFIX ?= arm-none-eabi-
CC := $(PREFIX)gcc
OBJCOPY := $(PREFIX)objcopy
SIZE := $(PREFIX)size
HOST_CC ?= cc
PYTHON ?= python3

CPU_FLAGS := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
COMMON_FLAGS := $(CPU_FLAGS) -std=c11 -ffreestanding -fno-builtin \
                -ffunction-sections -fdata-sections -fstack-usage \
                -Wall -Wextra -Werror -Wshadow -Wundef -Wconversion \
                -Wdouble-promotion -Wformat=2
DEBUG_FLAGS ?= -Og -g3
DEFINES := -DSTM32F446xx
INCLUDES := -ICore/Inc -IPlatform/Inc -IApp/Inc -IProtocol/Inc -ITransport/Inc
CFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS) $(DEFINES) $(INCLUDES)
LDFLAGS := $(CPU_FLAGS) -nostdlib -Wl,--gc-sections \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
           -TSTM32F446RETX_FLASH.ld

SOURCES := \
    Core/Src/main.c \
    Core/Src/startup_stm32f446xx.c \
    Core/Src/syscalls.c \
    Platform/Src/platform.c \
    App/Src/app.c \
    App/Src/app_event.c \
    App/Src/sine_generator.c \
    Protocol/Src/protocol.c \
    Protocol/Src/protocol_crc.c \
    Transport/Src/serial_transport.c

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean validate host-test linker-layout-test

all: validate $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) STM32F446RETX_FLASH.ld
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

validate:
	$(PYTHON) Tools/validate_project.py

linker-layout-test:
	$(PYTHON) Tools/test_linker_layout.py

host-test:
	@mkdir -p $(BUILD_DIR)/host
	$(HOST_CC) -std=c11 -Wall -Wextra -Werror \
		-IProtocol/Inc \
		Protocol/Src/protocol.c Protocol/Src/protocol_crc.c \
		Tests/protocol_roundtrip_test.c \
		-o $(BUILD_DIR)/host/protocol_roundtrip_test
	$(BUILD_DIR)/host/protocol_roundtrip_test

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
