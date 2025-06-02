CXX = g++
CXXFLAGS = -std=c++17 -Wall -I./include -I./include/api/ -g
LDFLAGS = -pthread

# Directories
SRCDIR = src
TESTDIR = tests
BINDIR = bin

# Test directories
UNIT_TESTDIR = $(TESTDIR)/unit_tests
INTEGRATION_TESTDIR = $(TESTDIR)/integration_tests
SYSTEM_TESTDIR = $(TESTDIR)/system_tests

# Find all test sources
UNIT_TEST_SRCS := $(wildcard $(UNIT_TESTDIR)/*.cpp)
INTEGRATION_TEST_SRCS := $(wildcard $(INTEGRATION_TESTDIR)/*.cpp)
SYSTEM_TEST_SRCS := $(wildcard $(SYSTEM_TESTDIR)/*.cpp)

# Generate binary paths
UNIT_TEST_BINS := $(patsubst $(UNIT_TESTDIR)/%.cpp, $(BINDIR)/unit_tests/%, $(UNIT_TEST_SRCS))
INTEGRATION_TEST_BINS := $(patsubst $(INTEGRATION_TESTDIR)/%.cpp, $(BINDIR)/integration_tests/%, $(INTEGRATION_TEST_SRCS))
SYSTEM_TEST_BINS := $(patsubst $(SYSTEM_TESTDIR)/%.cpp, $(BINDIR)/system_tests/%, $(SYSTEM_TEST_SRCS))

# Main sources needed by tests
SRCS = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/core/*.cpp)

# Filter out main.cpp if it exists, as tests have their own main
SRCS := $(filter-out $(SRCDIR)/main.cpp, $(SRCS))

# Use a unique interface name to avoid conflicts
TEST_IFACE_NAME = test-dummy0

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
	run_integration_communicator_test \
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

# Compile test rules
$(BINDIR)/unit_tests/%: $(UNIT_TESTDIR)/%.cpp $(SRCS)
	@mkdir -p $(BINDIR)/unit_tests
	$(CXX) $(CXXFLAGS) -o $@ $< $(filter-out $<, $(filter %.cpp, $^)) $(LDFLAGS)

$(BINDIR)/integration_tests/%: $(INTEGRATION_TESTDIR)/%.cpp $(SRCS)
	@mkdir -p $(BINDIR)/integration_tests
	$(CXX) $(CXXFLAGS) -o $@ $< $(filter-out $<, $(filter %.cpp, $^)) $(LDFLAGS)

$(BINDIR)/system_tests/%: $(SYSTEM_TESTDIR)/%.cpp $(SRCS)
	@mkdir -p $(BINDIR)/system_tests
	$(CXX) $(CXXFLAGS) -o $@ $< $(filter-out $<, $(filter %.cpp, $^)) $(LDFLAGS)

# Run test groups
.PHONY: run_unit_tests
run_unit_tests: setup_dummy_iface
	@echo "Running unit tests..."
	@for test in $(UNIT_TEST_BINS); do \
		echo "\n=== Running unit test $$test ==="; \
		sudo ./$$test $(ARGS); \
		if [ $$? -ne 0 ]; then \
			echo "Unit test $$test failed!"; \
			make clean_iface; \
			exit 1; \
		fi; \
	done
	@echo "All unit tests completed successfully!"

.PHONY: run_integration_tests
run_integration_tests: setup_dummy_iface
	@echo "Running integration tests..."
	@for test in $(INTEGRATION_TEST_BINS); do \
		echo "\n=== Running integration test $$test ==="; \
		sudo ./$$test $(ARGS); \
		if [ $$? -ne 0 ]; then \
			echo "Integration test $$test failed!"; \
			make clean_iface; \
			exit 1; \
		fi; \
	done
	@echo "All integration tests completed successfully!"

.PHONY: run_system_tests
run_system_tests: setup_dummy_iface
	@echo "Running system tests..."
	@for test in $(SYSTEM_TEST_BINS); do \
		echo "\n=== Running system test $$test ==="; \
		TEST_LOG_FILE="tests/logs/`basename $$test`.log"; \
		sudo ./$$test $(ARGS) > $$TEST_LOG_FILE 2>&1; \
		if [ $$? -ne 0 ]; then \
			echo "System test $$test failed! Check $$TEST_LOG_FILE for details"; \
			cat $$TEST_LOG_FILE; \
			make clean_iface; \
			exit 1; \
		else \
			echo "System test `basename $$test` completed successfully. Log saved to $$TEST_LOG_FILE"; \
		fi; \
	done
	@make clean_iface
	@echo "All system tests completed successfully!"

# Run specific test
.PHONY: run_unit_%
run_unit_%: $(BINDIR)/unit_tests/%
	make setup_dummy_iface
	sudo ./$< $(ARGS)
	make clean_iface

.PHONY: run_integration_%
run_integration_%: $(BINDIR)/integration_tests/%
	make setup_dummy_iface
	sudo ./$< $(ARGS)
	make clean_iface

# Run a test with Valgrind memory check
.PHONY: run_integration_%_valgrind
run_integration_%_valgrind: $(BINDIR)/integration_tests/%
	make setup_dummy_iface
	@echo "Running Valgrind memory check on $<..."
	sudo valgrind --leak-check=full --show-leak-kinds=all ./$< $(ARGS)
	make clean_iface

.PHONY: run_system_%
run_system_%: $(BINDIR)/system_tests/%
	@make setup_dummy_iface
	@mkdir -p $(TESTDIR)/logs
	@(sudo ./$< $(ARGS) > $(TESTDIR)/logs/$*.log 2>&1; \
	RESULT=$$?; \
	if [ $$RESULT -ne 0 ]; then \
		echo "System test $* failed! Check $(TESTDIR)/logs/$*.log for details"; \
		cat $(TESTDIR)/logs/$*.log; \
	else \
		echo "System test $* completed successfully. Log saved to $(TESTDIR)/logs/$*.log"; \
	fi; \
	make clean_iface; \
	exit $$RESULT)

# Cleanup
.PHONY: clean
clean:
	rm -rf $(BINDIR)
	rm -rf $(TESTDIR)/logs

# Documentation targets
.PHONY: docs
docs:
	@echo "Generating documentation with Doxygen..."
	@mkdir -p doc/doxygen
	doxygen
	@echo "Documentation generated in doc/doxygen/html/"
	@echo "Open doc/doxygen/html/index.html in your browser to view it."

.PHONY: docs-open
docs-open:
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "Doxygen found. Generating documentation..."; \
		$(MAKE) docs; \
		echo "Opening documentation in browser..."; \
		open doc/doxygen/html/index.html; \
	elif [ -f doc/doxygen/html/index.html ]; then \
		echo "Doxygen not found, but documentation exists. Opening existing documentation..."; \
		open doc/doxygen/html/index.html; \
	else \
		echo "Error: Doxygen command not found and documentation does not exist."; \
		echo "Please install doxygen or generate documentation manually."; \
	fi

.PHONY: clean-docs
clean-docs:
	@echo "Cleaning documentation..."
	rm -rf doc/doxygen

.PHONY: setup_dummy_iface
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
	mkdir -p $(TESTDIR)/logs
	# Export the interface name for tests to use
	echo "$(TEST_IFACE_NAME)" > $(TESTDIR)/logs/current_test_iface

.PHONY: clean_iface
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

# Docker commands
.PHONY: docker-build
docker-build:
	docker build -t cpp-vehicle-app .

.PHONY: docker-run
docker-run:
	docker run -it --privileged -v $(PWD):/app cpp-vehicle-app
