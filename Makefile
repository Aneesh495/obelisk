CC ?= clang
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wno-gnu-label-as-value -O3 -g -Iinclude
LDFLAGS ?= -lpthread

# Architecture detection
UNAME_M := $(shell uname -m)
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
    # Strict C11 hides libc interfaces such as CLOCK_MONOTONIC and MAP_ANONYMOUS.
    CFLAGS += -D_DEFAULT_SOURCE
endif

ifeq ($(UNAME_M),arm64)
    CFLAGS += -DOBELISK_TARGET_ARM64=1
else ifeq ($(UNAME_M),aarch64)
    CFLAGS += -DOBELISK_TARGET_ARM64=1
else ifeq ($(UNAME_M),x86_64)
    CFLAGS += -DOBELISK_TARGET_X86_64=1
endif

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

CORE_SRCS = $(SRC_DIR)/vm/vm.c \
            $(SRC_DIR)/vm/tape.c \
            $(SRC_DIR)/compiler/ir.c \
            $(SRC_DIR)/compiler/optimizer.c \
            $(SRC_DIR)/compiler/parser.c \
            $(SRC_DIR)/compiler/codegen.c \
            $(SRC_DIR)/jit/jit_arm64.c \
            $(SRC_DIR)/jit/jit_x86_64.c \
            $(SRC_DIR)/jit/jit_exec.c \
            $(SRC_DIR)/aot/aot_transpiler.c \
            $(SRC_DIR)/server/server.c \
            $(SRC_DIR)/server/session.c \
            $(SRC_DIR)/server/ring_buffer.c

CORE_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(CORE_SRCS))

TARGET = $(BIN_DIR)/obelisk

.PHONY: all clean test unit-tests integration-tests benchmarks check-style dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BUILD_DIR)/vm
	@mkdir -p $(BUILD_DIR)/compiler
	@mkdir -p $(BUILD_DIR)/jit
	@mkdir -p $(BUILD_DIR)/aot
	@mkdir -p $(BUILD_DIR)/server
	@mkdir -p $(BUILD_DIR)/tests

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(CORE_OBJS) $(BUILD_DIR)/main.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) -c $< -o $@

# Unit test runners
$(BIN_DIR)/test_vm: $(CORE_OBJS) tests/unit/test_vm.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_optimizer: $(CORE_OBJS) tests/unit/test_optimizer.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_jit: $(CORE_OBJS) tests/unit/test_jit.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/test_ring_buffer: $(CORE_OBJS) tests/unit/test_ring_buffer.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/bench_vm: $(CORE_OBJS) bench/bench_vm.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

unit-tests: dirs $(BIN_DIR)/test_vm $(BIN_DIR)/test_optimizer $(BIN_DIR)/test_jit $(BIN_DIR)/test_ring_buffer
	@echo "=== Running Unit Tests ==="
	@$(BIN_DIR)/test_vm
	@$(BIN_DIR)/test_optimizer
	@$(BIN_DIR)/test_jit
	@$(BIN_DIR)/test_ring_buffer
	@echo "All C unit tests passed cleanly."

integration-tests:
	@echo "=== Running Integration Tests ==="
	@python3 -m unittest discover -s tests/integration -p "test_*.py"
	@python3 -m unittest discover -s tests/regression -p "test_*.py"
	@echo "All Python integration and regression tests passed cleanly."

test: unit-tests integration-tests

benchmarks: dirs $(BIN_DIR)/bench_vm
	@echo "=== Running Micro-Benchmarks ==="
	@$(BIN_DIR)/bench_vm
	@echo "=== Running Macro-Benchmarks ==="
	@python3 bench/bench_queries.py

check-style:
	@echo "=== Checking for forbidden characters (no em-dashes) ==="
	@! grep -rn "—" src/ include/ bf/ tests/ docs/ tools/ sdk/ bench/ 2>/dev/null || (echo "Error: Em-dash found!" && exit 1)
	@echo "Clean: No forbidden characters detected."

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
