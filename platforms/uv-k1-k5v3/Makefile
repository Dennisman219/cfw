################################################################################
# UV-K1 / UV-K5(8) v3 platform configuration
#
# MCU: Puya PY32F071xB (Cortex-M0+, 128 KiB flash, 16 KiB RAM)
#
# The radio keeps the stock bootloader in the first 10 KiB of flash. The
# bootloader starts this firmware at 0x08002800, so the linker script and the
# vector-table relocation in boot/system_py32f071.c must both use that address.
################################################################################

TARGET := cfw_uv-k1-k5v3.elf

# Toolchain ------------------------------------------------------------------
TOOLCHAIN := arm-none-eabi-
CC        := $(TOOLCHAIN)gcc
CXX       := $(TOOLCHAIN)g++
LD        := $(TOOLCHAIN)g++
OBJCOPY   := $(TOOLCHAIN)objcopy
SIZE      := $(TOOLCHAIN)size

# Architecture ---------------------------------------------------------------
ARCH_FLAGS := -mcpu=cortex-m0plus -mthumb

# Use -Os because flash is precious. -g3 keeps debugging information even in
# optimized builds and does not change the code that is flashed.
OPT_FLAGS   := -Os
DEBUG_FLAGS := -g3 -gdwarf-4
WARN_FLAGS  := -Wall -Wextra

COMMON_FLAGS := $(ARCH_FLAGS) $(OPT_FLAGS) $(DEBUG_FLAGS) $(WARN_FLAGS) \
                -ffunction-sections -fdata-sections

CFLAGS   := $(COMMON_FLAGS) -std=gnu11
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 \
            -fno-exceptions -fno-rtti \
            -fno-threadsafe-statics -fno-use-cxa-atexit \
            -fno-unwind-tables -fno-asynchronous-unwind-tables
ASFLAGS  := $(ARCH_FLAGS) $(DEBUG_FLAGS) -x assembler-with-cpp

# Linker ---------------------------------------------------------------------
LDSCRIPT := $(PLATFORM_DIR)/py32f071xb.ld

# -nostartfiles: the startup file in boot/ provides the reset handler and the
#   vector table, so the toolchain startup must not be linked in.
# --gc-sections: drop functions and data that are never referenced.
# -z max-page-size=8: the application starts at 0x08002800, which is not page
#   aligned. With the default 4 KiB page size the ELF load segment starts at
#   0x08002000 and covers part of the bootloader. A tool that flashes the ELF
#   by segments could then overwrite the bootloader. A small alignment keeps
#   the ELF inside the application area; the .bin is unaffected either way.
LDFLAGS := $(ARCH_FLAGS) -nostartfiles \
           -Wl,--gc-sections \
           -Wl,--print-memory-usage \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
           -Wl,-z,max-page-size=8 \
           -Wl,--no-warn-rwx-segments

# nano.specs selects the small newlib C library. nosys.specs provides the
# "no operating system" stubs, so no syscall implementation is required.
LDLIBS := --specs=nano.specs --specs=nosys.specs -lc -lm -lgcc

# The exact MCU variant must be defined before the Puya headers are included.
PLATFORM_CPPFLAGS := -DPY32F071xB -DUSE_FULL_LL_DRIVER

PLATFORM_INCLUDES := \
	-isystem $(PLATFORM_DIR)/4_cmsis \
	-I$(PLATFORM_DIR)/3_hardware \
	-I$(PLATFORM_DIR)/2_hal \
	-I$(PLATFORM_DIR)/boot

# Sources --------------------------------------------------------------------
# Paths are relative to this platform directory.
PLATFORM_SRCS := \
	boot/startup_py32f071xx.s \
	boot/system_py32f071.c \
	2_hal/Battery.cpp \
	2_hal/Clock.cpp \
	2_hal/Display.cpp \
	2_hal/Platform.cpp \
	2_hal/Radio.cpp \
	2_hal/StatusLed.cpp \
	3_hardware/Adc1.cpp \
	3_hardware/Backlight.cpp \
	3_hardware/BK4819.cpp \
	3_hardware/Board.cpp \
	3_hardware/Fault.cpp \
	3_hardware/ST7565.cpp \
	3_hardware/SysTickTimer.cpp
