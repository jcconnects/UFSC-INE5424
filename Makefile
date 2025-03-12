CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic
INCLUDES = -Iinclude
SRCDIR = src
BUILDDIR = build
TARGET = $(BUILDDIR)/program

# Source files
SOURCES = $(shell find $(SRCDIR) -name "*.cpp")
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# Test files
TESTDIR = tests
TESTSOURCES = $(shell find $(TESTDIR) -name "*.cpp")
TESTOBJECTS = $(TESTSOURCES:$(TESTDIR)/%.cpp=$(BUILDDIR)/$(TESTDIR)/%.o)
TESTEXEC = $(BUILDDIR)/tests/runTests

# Create build directory structure
DIRS = $(sort $(dir $(OBJECTS) $(TESTOBJECTS)))
$(shell mkdir -p $(DIRS))

.PHONY: all clean test doc

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

test: $(TESTEXEC)
	./$(TESTEXEC)

$(TESTEXEC): $(TESTOBJECTS) $(filter-out $(BUILDDIR)/main.o, $(OBJECTS))
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(BUILDDIR)/$(TESTDIR)/%.o: $(TESTDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

doc:
	@echo "Generating documentation..."

clean:
	rm -rf $(BUILDDIR)/*
