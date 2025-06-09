# =============================================================================
# Enhanced Makefile for UFSC-INE5424 Communication Library
# =============================================================================

# Compiler and flags
CXX := g++
BASE_CXXFLAGS := -std=c++17 -I./include -I./include/api/ -g
LDFLAGS := -pthread

# Warning flags (can be customized)
WARNING_FLAGS := -Wall -Wextra -Wpedantic
SUPPRESS_FLAGS := -Wno-ignored-qualifiers -Wno-unused-parameter -Wno-deprecated-copy
SUPPRESS_FLAGS += -Wno-vla -Wno-unused-variable -Wno-unused-local-typedefs -Wno-c++20-extensions

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    PLATFORM_FLAGS := -DMACOS_BUILD
    SUPPRESS_FLAGS += -Wno-deprecated-declarations
else ifeq ($(UNAME_S),Linux)
    PLATFORM_FLAGS := -DLINUX_BUILD
else
    PLATFORM_FLAGS := -DGENERIC_BUILD
endif

# Combine flags
CXXFLAGS := $(BASE_CXXFLAGS) $(WARNING_FLAGS) $(SUPPRESS_FLAGS) $(PLATFORM_FLAGS)

# Build configurations
DEBUG_FLAGS := -DDEBUG -O0 -g3
RELEASE_FLAGS := -DNDEBUG -O3 -march=native
PROFILE_FLAGS := -pg -O2

# Default build type
BUILD_TYPE ?= debug

# Set flags based on build type
ifeq ($(BUILD_TYPE),release)
    CXXFLAGS += $(RELEASE_FLAGS)
else ifeq ($(BUILD_TYPE),profile)
    CXXFLAGS += $(PROFILE_FLAGS)
    LDFLAGS += -pg
else ifeq ($(BUILD_TYPE),strict)
    # Strict mode: show all warnings, treat warnings as errors
    CXXFLAGS := $(BASE_CXXFLAGS) $(WARNING_FLAGS) $(PLATFORM_FLAGS) $(DEBUG_FLAGS) -Werror
else
    CXXFLAGS += $(DEBUG_FLAGS)
endif

# =============================================================================
# Directory Structure
# =============================================================================
SRCDIR := src
INCDIR := include
TESTDIR := tests
BINDIR := bin
OBJDIR := build/obj
LIBDIR := build/lib
DOCDIR := doc

# Test directories
UNIT_TESTDIR := $(TESTDIR)/unit_tests
INTEGRATION_TESTDIR := $(TESTDIR)/integration_tests
SYSTEM_TESTDIR := $(TESTDIR)/system_tests

# =============================================================================
# Source Files Discovery
# =============================================================================

# For header-only library, we don't need separate source files
# Tests link directly against headers
LIB_SRCS := 
LIB_OBJS := 

# Test source files
UNIT_TEST_SRCS := $(wildcard $(UNIT_TESTDIR)/*.cpp)
INTEGRATION_TEST_SRCS := $(wildcard $(INTEGRATION_TESTDIR)/*.cpp)  
SYSTEM_TEST_SRCS := $(wildcard $(SYSTEM_TESTDIR)/*.cpp)

# Generate binary paths
UNIT_TEST_BINS := $(UNIT_TEST_SRCS:$(UNIT_TESTDIR)/%.cpp=$(BINDIR)/unit_tests/%)
INTEGRATION_TEST_BINS := $(INTEGRATION_TEST_SRCS:$(INTEGRATION_TESTDIR)/%.cpp=$(BINDIR)/integration_tests/%)
SYSTEM_TEST_BINS := $(SYSTEM_TEST_SRCS:$(SYSTEM_TESTDIR)/%.cpp=$(BINDIR)/system_tests/%)

# No library target needed for header-only design
# LIBRARY := $(LIBDIR)/libine5424.a

# Interface name for testing
TEST_IFACE_NAME := test-dummy0

# Default target: compile and run all tests
.PHONY: all
all: dirs \
	run_unit_buffer_test \
	run_unit_protocol_structure_test \
	run_unit_can_test \
	run_unit_clock_test \
	run_unit_rsu_test \
	run_unit_periodic_thread_test \
	run_unit_ethernet_test \
	run_unit_list_test \
	run_unit_radius_collision_test \
	run_unit_location_service_test \
	run_system_demo

# Compile and run all tests in the correct order
.PHONY: test
test: dirs unit_tests system_tests

# Compile all tests
.PHONY: compile_tests
compile_tests: dirs $(UNIT_TEST_BINS) $(INTEGRATION_TEST_BINS) $(SYSTEM_TEST_BINS)

# Create necessary directories
dirs:
	mkdir -p $(BINDIR)/unit_tests
	mkdir -p $(BINDIR)/integration_tests
	mkdir -p $(BINDIR)/system_tests
	mkdir -p $(TESTDIR)/logs

# Test targets
.PHONY: unit_tests
unit_tests: dirs $(UNIT_TEST_BINS) run_unit_tests

.PHONY: integration_tests
integration_tests: dirs $(INTEGRATION_TEST_BINS) run_integration_tests

.PHONY: system_tests
system_tests: dirs $(SYSTEM_TEST_BINS) run_system_tests

# Test binary compilation rules (header-only - no library objects needed)
$(BINDIR)/unit_tests/%: $(UNIT_TESTDIR)/%.cpp | $(BINDIR)/unit_tests
	@echo "Compiling unit test: $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/integration_tests/%: $(INTEGRATION_TESTDIR)/%.cpp | $(BINDIR)/integration_tests  
	@echo "Compiling integration test: $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/system_tests/%: $(SYSTEM_TESTDIR)/%.cpp | $(BINDIR)/system_tests
	@echo "Compiling system test: $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# =============================================================================
# Test Execution
# =============================================================================

test: unit_tests integration_tests system_tests ## Run all tests

unit_tests: $(UNIT_TEST_BINS) setup_dummy_iface ## Run unit tests
	@echo "Running unit tests..."
	@for test in $(UNIT_TEST_BINS); do \
		echo "\n=== Running unit test $$test ==="; \
		sudo ./$$test $(ARGS) || { echo "Unit test $$test failed!"; $(MAKE) clean_iface; exit 1; }; \
	done
	@echo "All unit tests completed successfully!"

integration_tests: $(INTEGRATION_TEST_BINS) setup_dummy_iface ## Run integration tests
	@echo "Running integration tests..."
	@for test in $(INTEGRATION_TEST_BINS); do \
		echo "\n=== Running integration test $$test ==="; \
		sudo ./$$test $(ARGS) || { echo "Integration test $$test failed!"; $(MAKE) clean_iface; exit 1; }; \
	done
	@echo "All integration tests completed successfully!"

system_tests: $(SYSTEM_TEST_BINS) setup_dummy_iface ## Run system tests
	@echo "Running system tests..."
	@for test in $(SYSTEM_TEST_BINS); do \
		echo "\n=== Running system test $$test ==="; \
		TEST_LOG_FILE="tests/logs/`basename $$test`.log"; \
		sudo ./$$test $(ARGS) > $$TEST_LOG_FILE 2>&1; \
		if [ $$? -ne 0 ]; then \
			echo "System test $$test failed! Check $$TEST_LOG_FILE for details"; \
			cat $$TEST_LOG_FILE; $(MAKE) clean_iface; exit 1; \
		else \
			echo "System test `basename $$test` completed successfully. Log saved to $$TEST_LOG_FILE"; \
		fi; \
	done
	@$(MAKE) clean_iface
	@echo "All system tests completed successfully!"

# =============================================================================
# Individual Test Runners
# =============================================================================

# Run specific unit test
run_unit_%: $(BINDIR)/unit_tests/% ## Run specific unit test
	@$(MAKE) setup_dummy_iface
	@sudo ./$< $(ARGS)
	@$(MAKE) clean_iface

# Run specific integration test  
run_integration_%: $(BINDIR)/integration_tests/% ## Run specific integration test
	@$(MAKE) setup_dummy_iface
	@sudo ./$< $(ARGS)
	@$(MAKE) clean_iface

# Run specific system test
run_system_%: $(BINDIR)/system_tests/% ## Run specific system test
	@$(MAKE) setup_dummy_iface
	@mkdir -p $(TESTDIR)/logs
	@(sudo ./$< $(ARGS) > $(TESTDIR)/logs/$*.log 2>&1; \
	RESULT=$$?; \
	if [ $$RESULT -ne 0 ]; then \
		echo "System test $* failed! Check $(TESTDIR)/logs/$*.log for details"; \
		cat $(TESTDIR)/logs/$*.log; \
	else \
		echo "System test $* completed successfully. Log saved to $(TESTDIR)/logs/$*.log"; \
	fi; \
	$(MAKE) clean_iface; \
	exit $$RESULT)

# =============================================================================
# Analysis and Debug Tools
# =============================================================================

# Valgrind memory check
valgrind_%: $(BINDIR)/integration_tests/% ## Run test with Valgrind
	@$(MAKE) setup_dummy_iface
	@echo "Running Valgrind memory check on $<..."
	@sudo valgrind --leak-check=full --show-leak-kinds=all ./$< $(ARGS)
	@$(MAKE) clean_iface

# Thread analysis
thread_analysis: ## Run thread analysis using external tools  
	@echo "Running thread analysis..."
	@chmod +x tools/thread_analysis/run_thread_analysis.sh
	@./tools/thread_analysis/run_thread_analysis.sh

# Code coverage (requires gcov support)
coverage: CXXFLAGS += --coverage
coverage: LDFLAGS += --coverage  
coverage: clean compile_tests test ## Generate code coverage report
	@echo "Generating coverage report..."
	@echo "Note: Coverage for header-only library focuses on test execution paths"
	@mkdir -p $(BINDIR)/coverage
	@mv *.gcov $(BINDIR)/coverage/ 2>/dev/null || true
	@echo "Coverage files moved to $(BINDIR)/coverage/"

# =============================================================================
# Documentation
# =============================================================================

docs: ## Generate documentation with Doxygen
	@echo "Generating documentation with Doxygen..."
	@mkdir -p $(DOCDIR)/doxygen
	@doxygen
	@echo "Documentation generated in $(DOCDIR)/doxygen/html/"

docs-open: docs ## Generate and open documentation
	@echo "Opening documentation in browser..."
	@open $(DOCDIR)/doxygen/html/index.html

clean-docs: ## Clean generated documentation
	@echo "Cleaning documentation..."
	@rm -rf $(DOCDIR)/doxygen

# =============================================================================
# Build Management
# =============================================================================

# Create necessary directories
dirs: | $(BINDIR)/unit_tests $(BINDIR)/integration_tests $(BINDIR)/system_tests $(TESTDIR)/logs

$(BINDIR)/unit_tests $(BINDIR)/integration_tests $(BINDIR)/system_tests $(TESTDIR)/logs:
	@mkdir -p $@

# Clean build artifacts
clean: ## Clean all build artifacts
	@echo "Cleaning build artifacts..."
	@rm -rf $(BINDIR)
	@rm -rf $(TESTDIR)/logs
	@rm -f *.gcov *.gcda *.gcno

# Deep clean (including documentation)
distclean: clean clean-docs ## Deep clean including documentation
	@echo "Deep clean completed"

# =============================================================================
# Network Interface Management  
# =============================================================================

setup_dummy_iface:
	@if ip link show $(TEST_IFACE_NAME) > /dev/null 2>&1; then \
		echo "Interface $(TEST_IFACE_NAME) already exists, checking type..."; \
		if ip link show $(TEST_IFACE_NAME) | grep -q "dummy"; then \
			echo "Existing $(TEST_IFACE_NAME) is a dummy interface, reusing it."; \
		else \
			echo "WARNING: $(TEST_IFACE_NAME) exists but is NOT a dummy interface. Using a different name."; \
			export TEST_IFACE_NAME="test-dummy1"; \
			if ip link show $$TEST_IFACE_NAME > /dev/null 2>&1; then \
				echo "Interface $$TEST_IFACE_NAME also exists. Please clean up interfaces manually."; \
				exit 1; \
			fi; \
			echo "Creating interface $$TEST_IFACE_NAME..."; \
			sudo ip link add $$TEST_IFACE_NAME type dummy; \
			sudo ip link set $$TEST_IFACE_NAME up; \
		fi; \
	else \
		echo "Creating interface $(TEST_IFACE_NAME)..."; \
		sudo ip link add $(TEST_IFACE_NAME) type dummy; \
		sudo ip link set $(TEST_IFACE_NAME) up; \
	fi; \
	mkdir -p $(TESTDIR)/logs; \
	echo "$(TEST_IFACE_NAME)" > $(TESTDIR)/logs/current_test_iface

clean_iface:
	@if [ -f $(TESTDIR)/logs/current_test_iface ]; then \
		TEST_IFACE=$$(cat $(TESTDIR)/logs/current_test_iface); \
		if ip link show $$TEST_IFACE > /dev/null 2>&1; then \
			if ip link show $$TEST_IFACE | grep -q "dummy"; then \
				echo "Removing dummy interface $$TEST_IFACE..."; \
				sudo ip link delete $$TEST_IFACE type dummy; \
			else \
				echo "WARNING: $$TEST_IFACE exists but is NOT a dummy interface. Not removing."; \
			fi; \
		fi; \
		rm -f $(TESTDIR)/logs/current_test_iface; \
	fi

# =============================================================================
# Docker Support
# =============================================================================

docker-build: ## Build Docker image
	@docker build -t cpp-vehicle-app .

docker-run: ## Run in Docker container  
	@docker run -it --privileged -v $(PWD):/app cpp-vehicle-app

# =============================================================================
# Development Utilities
# =============================================================================

# Show build configuration
config: ## Show current build configuration
	@echo "Build Configuration:"
	@echo "  Build Type: $(BUILD_TYPE)"
	@echo "  Platform: $(UNAME_S)"
	@echo "  Compiler: $(CXX)"
	@echo "  Base Flags: $(BASE_CXXFLAGS)"
	@echo "  Warning Flags: $(WARNING_FLAGS)"
	@echo "  Suppress Flags: $(SUPPRESS_FLAGS)"
	@echo "  Platform Flags: $(PLATFORM_FLAGS)"
	@echo "  Final CXXFLAGS: $(CXXFLAGS)"
	@echo "  Linker Flags: $(LDFLAGS)"
	@echo "  Source Files: $(words $(LIB_SRCS)) files"
	@echo "  Test Files: $(words $(UNIT_TEST_SRCS)) unit, $(words $(INTEGRATION_TEST_SRCS)) integration, $(words $(SYSTEM_TEST_SRCS)) system"

# Warning level targets
compile_quiet: SUPPRESS_FLAGS += -w
compile_quiet: compile_tests ## Compile with all warnings suppressed

compile_strict: BUILD_TYPE := strict  
compile_strict: compile_tests ## Compile with strict warnings (treat warnings as errors)

compile_verbose: WARNING_FLAGS += -Wconversion -Wshadow -Wfloat-equal
compile_verbose: compile_tests ## Compile with extra verbose warnings

# Install development dependencies (example)
install-deps: ## Install development dependencies
	@echo "Installing development dependencies..."
	@echo "Platform: $(UNAME_S)"
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		echo "macOS detected - checking for Homebrew..."; \
		if command -v brew >/dev/null 2>&1; then \
			echo "Homebrew found, installing dependencies..."; \
			brew install doxygen valgrind || echo "Some packages may not be available on macOS"; \
		else \
			echo "Homebrew not found. Please install Homebrew first."; \
		fi; \
	elif [ "$(UNAME_S)" = "Linux" ]; then \
		echo "Linux detected - using package manager..."; \
		if command -v apt-get >/dev/null 2>&1; then \
			sudo apt-get update && sudo apt-get install -y doxygen valgrind build-essential; \
		elif command -v yum >/dev/null 2>&1; then \
			sudo yum install -y doxygen valgrind gcc-c++; \
		else \
			echo "Please install doxygen, valgrind, and build tools manually"; \
		fi; \
	fi
	@echo "Dependencies installation completed"

# Format code (if you have clang-format)
format: ## Format code using clang-format
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "Formatting code..."; \
		find include tests -name "*.h" -o -name "*.cpp" | xargs clang-format -i; \
	else \
		echo "clang-format not found. Please install it for code formatting."; \
	fi

# =============================================================================
# Debug and Analysis
# =============================================================================

# Print variables for debugging
debug-vars: ## Print Makefile variables for debugging
	@echo "SRCDIR: $(SRCDIR)"
	@echo "LIB_SRCS: $(LIB_SRCS)"  
	@echo "LIB_OBJS: $(LIB_OBJS)"
	@echo "UNIT_TEST_SRCS: $(UNIT_TEST_SRCS)"
	@echo "UNIT_TEST_BINS: $(UNIT_TEST_BINS)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"

# Platform compatibility check
platform-check: ## Check platform compatibility
	@echo "Platform Compatibility Check:"
	@echo "  OS: $(UNAME_S)"
	@echo "  Compiler: $(shell $(CXX) --version 2>/dev/null | head -1 || echo "Not found")"
	@echo "  Make: $(shell make --version 2>/dev/null | head -1 || echo "Not found")"
	@if [ "$(UNAME_S)" = "Darwin" ]; then \
		echo "  Note: This project is designed for Linux. Some features may not work on macOS."; \
		echo "  Missing: epoll, eventfd, Linux-specific networking"; \
		echo "  Suggestion: Use Docker or a Linux VM for full compatibility"; \
	elif [ "$(UNAME_S)" = "Linux" ]; then \
		echo "  Status: Full compatibility expected"; \
	else \
		echo "  Warning: Untested platform"; \
	fi 