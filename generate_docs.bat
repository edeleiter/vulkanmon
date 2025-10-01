@echo off
REM VulkanMon Documentation Generation Script
REM Generates API documentation using Doxygen

echo =============================================================================
echo VulkanMon API Documentation Generation
echo =============================================================================

REM Check if we're in the build directory
if not exist "CMakeCache.txt" (
    echo Error: This script must be run from the build directory
    echo Please run: cd build && ..\generate_docs.bat
    pause
    exit /b 1
)

REM Check if Doxygen is available
where doxygen >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Doxygen not found in PATH
    echo Please install Doxygen and restart your terminal
    echo Download from: https://www.doxygen.nl/download.html
    pause
    exit /b 1
)

echo Found Doxygen:
doxygen --version

echo.
echo Generating VulkanMon API documentation...
echo.

REM Generate documentation using CMake target
cmake --build . --target docs

if %ERRORLEVEL% equ 0 (
    echo.
    echo =============================================================================
    echo SUCCESS: Documentation generated successfully!
    echo =============================================================================
    echo.
    echo Documentation location: build\docs\html\index.html
    echo.
    set /p choice="Open documentation in browser? [Y/n]: "
    if /i "%choice%"=="" set choice=Y
    if /i "%choice%"=="Y" (
        start docs\html\index.html
    )
) else (
    echo.
    echo =============================================================================
    echo ERROR: Documentation generation failed!
    echo =============================================================================
    echo.
    echo Check the output above for error details.
    echo Common issues:
    echo - Doxygen not in PATH (restart terminal after installation)
    echo - Missing source files or documentation syntax errors
    echo.
)

pause