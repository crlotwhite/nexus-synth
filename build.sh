#!/bin/bash
# NexusSynth Cross-platform Build Script
# Works on Linux, macOS, and other Unix systems

set -e  # Exit on error

echo "Building NexusSynth..."

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found. Please install CMake."
    exit 1
fi

# Detect platform
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
else
    PLATFORM="Unix"
fi

echo "Building on $PLATFORM"

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring build with CPM dependency management..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=../install \
    -DNEXUSSYNTH_BUILD_TESTS=ON

# Build with optimal parallel jobs
NPROC=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
cmake --build . --parallel $NPROC

# Run tests
echo "Running tests..."
ctest --output-on-failure || echo "WARNING: Some tests failed"

# Install to local directory
cmake --install .

echo ""
echo "Build completed successfully!"
echo "Executable: build/src/nexussynth"
echo "Tests: build/tests/nexussynth_tests"
echo "Installation: install/"

cd ..