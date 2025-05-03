# Top-level Makefile with debug support

# Configuration
MODULES := state chat public gate file
DEMO_DIR := demo
TEST_DIR := test
BUILD_DIR := build
RUN_TARGETS := $(addprefix run-,$(MODULES))
TEST_TARGETS := $(addprefix test-,$(MODULES))
DEBUG_TARGETS := $(addprefix debug-,$(MODULES))

# Default target
all: build

# Build all modules
build:
	@echo "Building all modules..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake ..
	@$(MAKE) -C $(BUILD_DIR)
	@for module in $(MODULES); do \
		echo "Building $$module..."; \
		mkdir -p $$module/$(BUILD_DIR); \
		cd $$module/$(BUILD_DIR) && cmake .. && $(MAKE); \
		cd ../..; \
	done

# Clean all builds
clean:
	@echo "Cleaning builds..."
	@rm -rf $(BUILD_DIR)
	@for module in $(MODULES) $(TEST_DIR); do \
		rm -rf $$module/$(BUILD_DIR); \
	done

# Run targets with arguments
$(RUN_TARGETS): run-%:
	@echo "Running $* with arguments: $(ARGS)"
	@cd $*/$(BUILD_DIR) && ./$* $(ARGS)

# Debug targets - launch with gdb
$(DEBUG_TARGETS): debug-%:
	@echo "Debugging $* with arguments: $(ARGS)"
	@cd $*/$(BUILD_DIR) && gdb --args ./$* $(ARGS)

# Test targets
$(TEST_TARGETS): test-%:
	@echo "Testing $*..."
	@cd $*/$(BUILD_DIR) && ctest --output-on-failure

# Top-level test run
run-test:
	@echo "Running top-level tests..."
	@cd $(TEST_DIR)/$(BUILD_DIR) && ctest --output-on-failure

# Individual module targets
state: build-state run-state
chat: build-chat run-chat
public: build-public run-public
gate: build-gate run-gate
file: build-file run-file

# Build individual modules
build-%:
	@echo "Building $*..."
	@mkdir -p $*/$(BUILD_DIR)
	@cd $*/$(BUILD_DIR) && cmake .. && $(MAKE)

# Special targets with common arguments(TODO)
gate-with-args: ARGS = ../config.yaml --info
chat-with-args-1: ARGS = run-chat ../config.yaml --normal --info
chat-with-args-2: ARGS = run-chat ../config2.yaml --normal --info


# Phony targets
.PHONY: all build clean run-test $(RUN_TARGETS) $(TEST_TARGETS) $(DEBUG_TARGETS) \
        $(MODULES) gate-with-args chat-with-args-1 chat-with-args-2
