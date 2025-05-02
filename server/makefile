# Top-level Makefile for CMake modules

# Configuration
MODULES := state chat public gate file
DEMO_DIR := demo
TEST_DIR := test
BUILD_DIR := build
RUN_TARGETS := $(addprefix run-,$(MODULES))
TEST_TARGETS := $(addprefix test-,$(MODULES))

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

# Run targets for each module
$(RUN_TARGETS): run-%:
	@echo "Running $*..."
	@cd $*/$(BUILD_DIR) && ./$*

# Test targets for each module
$(TEST_TARGETS): test-%:
	@echo "Testing $*..."
	@cd $*/$(BUILD_DIR)/test && ctest --output-on-failure

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

# Phony targets
.PHONY: all build clean run-t	est $(RUN_TARGETS) $(TEST_TARGETS) $(MODULES)