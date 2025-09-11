#!/bin/bash

# VulkanMon Build and Run Script
# Builds the project and runs the debug version

set -e  # Exit on any error

echo "=== VulkanMon Build and Run ==="

# Clean and rebuild
echo "Step 1: Cleaning previous build..."
rm -rf build
rm -f shaders/*.spv

echo "Step 2: Configuring project..."
cmake --preset dev-windows

echo "Step 3: Building project..."
cmake --build build --config Debug --parallel

echo "Step 4: Running application..."
./build/Debug/vulkanmon.exe

echo "Build and run completed!"