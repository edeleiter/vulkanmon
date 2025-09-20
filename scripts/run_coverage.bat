@echo off
REM Simple test coverage script - following "Simple is Powerful" philosophy
REM Usage: scripts\run_coverage.bat

echo ========================================
echo VulkanMon Test Coverage Runner
echo ========================================

REM Create coverage build directory
if not exist build_coverage mkdir build_coverage
cd build_coverage

echo.
echo [1/4] Configuring with coverage enabled...
cmake .. -DENABLE_COVERAGE=ON
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

echo.
echo [2/4] Building tests with coverage...
cmake --build . --config Debug
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

echo.
echo [3/4] Running tests to generate coverage data...
cd tests_cpp
Debug\vulkanmon_tests.exe
if errorlevel 1 (
    echo ERROR: Tests failed
    exit /b 1
)

echo.
echo [4/4] Coverage data generated successfully!
echo.
echo Coverage files created in: build_coverage\tests_cpp\Debug\
echo - Look for .pdb and other coverage files
echo.
echo Next steps:
echo 1. Use Visual Studio Code Coverage tools to analyze
echo 2. Or use command-line tools like vsperf for reporting
echo.
echo ========================================
echo Coverage run completed successfully!
echo ========================================

cd ..\..