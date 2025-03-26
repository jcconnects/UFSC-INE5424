CXX = g++
CXXFLAGS = -std=c++14 -Wall -I./include

# All source files
SRCS = $(wildcard src/*.cpp) $(wildcard src/core/*.cpp)

# Test sources
TEST_COMMUNICATOR_SRC = tests/test_communicator.cpp
TEST_INITIALIZER_SRC = tests/test_initializer.cpp

# Output binaries
TEST_COMMUNICATOR_BIN = bin/test_communicator
TEST_INITIALIZER_BIN = bin/test_initializer

# Build all tests
all: dirs $(TEST_COMMUNICATOR_BIN) $(TEST_INITIALIZER_BIN)

# Create directories
dirs:
	mkdir -p bin

# Test targets
$(TEST_COMMUNICATOR_BIN): $(TEST_COMMUNICATOR_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< -pthread

$(TEST_INITIALIZER_BIN): $(TEST_INITIALIZER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< -pthread

# Run test targets
run_test_communicator: $(TEST_COMMUNICATOR_BIN)
	./$(TEST_COMMUNICATOR_BIN)

run_test_initializer: $(TEST_INITIALIZER_BIN)
	./$(TEST_INITIALIZER_BIN) 1 100 -v

# Clean
clean:
	rm -rf bin/*

.PHONY: all clean dirs run_test_communicator run_test_initializer
