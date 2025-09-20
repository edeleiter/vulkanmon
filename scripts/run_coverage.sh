#!/bin/bash
# Simple test coverage script - following "Simple is Powerful" philosophy
# Usage: scripts/run_coverage.sh

echo "========================================"
echo "VulkanMon Test Coverage Runner"
echo "========================================"

# Create coverage build directory
mkdir -p build_coverage
cd build_coverage

echo
echo "[1/4] Configuring with coverage enabled..."
cmake .. -DENABLE_COVERAGE=ON
if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    exit 1
fi

echo
echo "[2/4] Building tests with coverage..."
cmake --build . --config Debug
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    exit 1
fi

echo
echo "[3/4] Running tests to generate coverage data..."
cd tests_cpp
./Debug/vulkanmon_tests || Debug/vulkanmon_tests.exe
if [ $? -ne 0 ]; then
    echo "ERROR: Tests failed"
    exit 1
fi

echo
echo "[4/4] Coverage data generated successfully!"
echo
echo "Coverage files created in: build_coverage/tests_cpp/Debug/"
echo "- Look for coverage data files"
echo
echo "Next steps:"
echo "1. Use appropriate coverage tools for your compiler"
echo "2. Generate HTML reports for analysis"
echo
echo "========================================"
echo "Coverage run completed successfully!"
echo "========================================"

cd ../..