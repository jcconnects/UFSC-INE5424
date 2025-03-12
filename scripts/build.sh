#!/bin/bash

# Exit on error
set -e

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

echo "Build completed successfully!"
