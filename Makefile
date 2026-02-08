# ===== Project =====
TARGET := rsu

# ===== Toolchain =====
CC := gcc

# ===== Paths =====
SRC_DIR := src
INC_DIR := inc
BUILD_DIR := build

# ===== Source discovery =====
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# ===== Include paths =====
# - if include/ exists, use it
# - also add src/ and current dir (helpful when headers are placed flat)
INCLUDES := -I. -I$(SRC_DIR)
ifeq ($(wildcard $(INC_DIR)),$(INC_DIR))
  INCLUDES += -I$(INC_DIR)
endif

# ===== Flags =====
CSTD   := -std=c11
WARN   := -Wall -Wextra
OPT    := -O2
DEBUG  := -g
DEFFLAGS := -D_POSIX_C_SOURCE=200809L

# If you want to treat warnings as errors, enable below:
# WERR := -Werror
WERR :=

CFLAGS  := $(CSTD) $(WARN) $(WERR) $(OPT) $(DEBUG) $(INCLUDES) $(DEFFLAGS) -MMD -MP
LDFLAGS := -pthread
LDLIBS  := -lgpiod

# ===== Default target =====
.PHONY: all
all: $(TARGET)

# ===== Link =====
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

# ===== Compile =====
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ===== Build dir =====
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ===== Clean =====
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# ===== Run (sudo for gpio) =====
.PHONY: run
run: $(TARGET)
	@echo "[RUN] sudo ./$(TARGET)"
	sudo ./$(TARGET)

# ===== Debug run =====
.PHONY: gdb
gdb: $(TARGET)
	sudo gdb --args ./$(TARGET)

# ===== Print vars (for troubleshooting) =====
.PHONY: print-vars
print-vars:
	@echo "TARGET    = $(TARGET)"
	@echo "SRCS      = $(SRCS)"
	@echo "OBJS      = $(OBJS)"
	@echo "INCLUDES  = $(INCLUDES)"
	@echo "CFLAGS    = $(CFLAGS)"
	@echo "LDFLAGS   = $(LDFLAGS)"
	@echo "LDLIBS    = $(LDLIBS)"

# ===== Dependency includes =====
-include $(DEPS)
