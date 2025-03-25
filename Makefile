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

.PHONY: all clean test doc directories observer_test run_observer_test

all: $(TARGET) observer_test

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
build_test_observer: $(BUILDDIR)/test_observer

$(BUILDDIR)/test_observer: $(BUILDDIR)/test_observer.o
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ -pthread

$(BUILDDIR)/test_observer.o: $(TESTDIR)/test_observer.cpp $(INCDIR)/observer.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

test_observer: $(BUILDDIR)/test_observer
	./$(BUILDDIR)/test_observer
