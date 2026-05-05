PLATFORM ?= uv-k1-k5v3
PROJECT_ROOT := $(shell pwd)
PLATFORM_DIR := $(PROJECT_ROOT)/platforms/$(PLATFORM)
BUILD_DIR := $(PROJECT_ROOT)/build/$(PLATFORM)
SRC_DIR := $(PROJECT_ROOT)/src
OBJ_DIR := $(BUILD_DIR)

# Include platform configuration
include $(PLATFORM_DIR)/Makefile

# Discover source files
PORTABLE_SRCS := $(shell find $(SRC_DIR) -type f \( -name "*.cpp" -o -name "*.c" \))
ALL_SRCS := $(PORTABLE_SRCS) $(PLATFORM_SRCS)

# Convert to object files
PORTABLE_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/src/%.o,$(filter %.cpp,$(PORTABLE_SRCS)))
PORTABLE_OBJS += $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/src/%.o,$(filter %.c,$(PORTABLE_SRCS)))

PLATFORM_OBJS := $(patsubst $(PLATFORM_DIR)/%.cpp,$(OBJ_DIR)/plat/%.o,$(filter %.cpp,$(PLATFORM_SRCS)))
PLATFORM_OBJS += $(patsubst $(PLATFORM_DIR)/%.c,$(OBJ_DIR)/plat/%.o,$(filter %.c,$(PLATFORM_SRCS)))
PLATFORM_OBJS += $(patsubst $(PLATFORM_DIR)/%.s,$(OBJ_DIR)/plat/%.o,$(filter %.s,$(PLATFORM_SRCS)))

ALL_OBJS := $(PORTABLE_OBJS) $(PLATFORM_OBJS)

# Include paths
INCLUDES := -I$(SRC_DIR) -I$(SRC_DIR)/0_app -I$(SRC_DIR)/1_logic -I$(SRC_DIR)/2_hal \
            -I$(PLATFORM_DIR) -I$(PLATFORM_DIR)/2_hal -I$(PLATFORM_DIR)/3_hpl -I$(PLATFORM_DIR)/4_pal -I$(PLATFORM_DIR)/boot

# ============================================================================
# Targets
# ============================================================================

.PHONY: all clean info

all: $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET): $(ALL_OBJS)
	mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -T$(LDSCRIPT) -o $@ $(ALL_OBJS) $(LDLIBS)

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/src/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/plat/%.o: $(PLATFORM_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/plat/%.o: $(PLATFORM_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(INCLUDES) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/plat/%.o: $(PLATFORM_DIR)/%.s
	mkdir -p $(dir $@)
	$(CC) $(INCLUDES) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
