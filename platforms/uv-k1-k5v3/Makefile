################################################################################
# UV-K1-K5v3 Platform (PY32F071) - Platform Configuration
################################################################################

TOOLCHAIN := arm-none-eabi-
TARGET := cfw_uv-k1-k5v3.elf

# Compiler tools
CC := $(TOOLCHAIN)gcc
CXX := $(TOOLCHAIN)g++
LD := $(TOOLCHAIN)g++

# Common architecture flags
ARCH_FLAGS := -mcpu=cortex-m0plus -mthumb

# Compiler flags for ARM Cortex-M0+
CFLAGS := $(ARCH_FLAGS) -Os -g3 -std=c99 -ffunction-sections -fdata-sections
CXXFLAGS := $(ARCH_FLAGS) -Os -g3 -std=c++17 -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections
ASFLAGS := $(ARCH_FLAGS) -g3 -x assembler-with-cpp

# Linker configuration
LDSCRIPT := platforms/uv-k1-k5v3/py32f071xb.ld
LDFLAGS := $(ARCH_FLAGS) -nostartfiles -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(TARGET).map -Wl,--no-warn-rwx-segments
LDLIBS := -lc -lm -lnosys

# Platform-specific source files
PLATFORM_SRCS := \
	$(PROJECT_ROOT)/platforms/uv-k1-k5v3/boot/startup_py32f071xx.s \
	$(PROJECT_ROOT)/platforms/uv-k1-k5v3/boot/py32f071xx_init.c
