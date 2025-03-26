CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
INCLUDES = -Iinclude

SRCDIR = src
INCDIR = include
BUILDDIR = build
TESTDIR = tests

SOURCES = $(wildcard $(SRCDIR)/*.cpp) $(wildcard $(SRCDIR)/core/*.cpp)
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(SOURCES))

TARGET = $(BUILDDIR)/main

DIRS = $(dir $(OBJECTS))
$(shell mkdir -p $(DIRS))

.PHONY: all clean test doc directories observer_test run_observer_test initializer_test run_initializer_test communicator_test run_communicator_test

all: $(TARGET) observer_test initializer_test communicator_test

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

test: $(TARGET)
	./$(TARGET)

doc:
	doxygen

clean:
	rm -rf $(BUILDDIR)/*

# Observer Test specific rules
build_observer_test: $(BUILDDIR)/test_observer

$(BUILDDIR)/test_observer: $(BUILDDIR)/test_observer.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ -pthread

$(BUILDDIR)/test_observer.o: $(TESTDIR)/test_observer.cpp $(INCDIR)/observer.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run_observer_test: $(BUILDDIR)/test_observer
	./$(BUILDDIR)/test_observer

# Initializer Test rules
initializer_test: $(BUILDDIR)/test_initializer

$(BUILDDIR)/test_initializer: $(BUILDDIR)/test_initializer.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ -pthread

$(BUILDDIR)/test_initializer.o: $(TESTDIR)/test_initializer.cpp $(INCDIR)/initializer.h $(INCDIR)/vehicle.h $(INCDIR)/stubs/stub.h
	mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run_initializer_test: initializer_test
	./$(BUILDDIR)/test_initializer 3 1000 -v

# Communicator Test rules
communicator_test: $(BUILDDIR)/test_communicator

$(BUILDDIR)/test_communicator: $(BUILDDIR)/test_communicator.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ -pthread

$(BUILDDIR)/test_communicator.o: $(TESTDIR)/test_communicator.cpp $(INCDIR)/communicator.h $(INCDIR)/stubs/communicator_stubs.h $(INCDIR)/observer.h $(INCDIR)/observed.h
	mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run_communicator_test: communicator_test
	./$(BUILDDIR)/test_communicator
