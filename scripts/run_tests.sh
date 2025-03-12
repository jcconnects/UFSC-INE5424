#!/bin/bash

# Exit on error
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Configure with CMake if not already configured
if [ ! -f "Makefile" ]; then
    cmake ..
fi

# Build tests
cmake --build . --target test

# Run tests
ctest --verbose

echo "Tests completed!"
