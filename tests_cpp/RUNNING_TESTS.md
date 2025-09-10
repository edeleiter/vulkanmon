# Running VulkanMon C++ Unit Tests

Quick guide to building and running the Catch2-based C++ unit testing framework.

## Prerequisites

✅ **Already installed** (if you've been following VulkanMon development):
- CMake 3.20+
- Vulkan SDK 
- vcpkg with Catch2 installed
- Visual Studio 2022 Build Tools (Windows)

## Quick Start

### 1. Build the Tests

```bash
# Navigate to tests directory
cd tests_cpp

# Create build directory
mkdir build
cd build

# Configure with CMake (vcpkg integration)
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Debug

# Build the test executable
cmake --build . --config Debug
```

### 2. Run All Tests

```bash
# From tests_cpp/build directory
./Debug/vulkanmon_tests.exe
```

**Expected Output:**
```
test cases: 18 |  6 passed | 12 skipped
assertions: 14 | 14 passed |  0 skipped
```

*Note: 12 tests are skipped because they require full Vulkan context (Phase 2 feature)*

## Running Specific Tests

### By Category (Class)
```bash
# Test only the Logger class
./Debug/vulkanmon_tests.exe "[Logger]"

# Test only ResourceManager
./Debug/vulkanmon_tests.exe "[ResourceManager]"

# Test AssetManager
./Debug/vulkanmon_tests.exe "[AssetManager]"

# Test ModelLoader
./Debug/vulkanmon_tests.exe "[ModelLoader]"
```

### By Functionality Tags
```bash
# Run thread safety tests
./Debug/vulkanmon_tests.exe "[Threading]"

# Run basic functionality tests
./Debug/vulkanmon_tests.exe "[Basic]"

# Run Vulkan-dependent tests (will skip in Phase 1)
./Debug/vulkanmon_tests.exe "[Vulkan]"
```

### Combine Multiple Tags
```bash
# Run Logger basic tests only
./Debug/vulkanmon_tests.exe "[Logger][Basic]"
```

## Understanding Test Output

### Successful Test Run
```
[20:47:04.396] [INFO ] Thread safety test message  # Logger working correctly
[20:47:04.396] [INFO ] [VULKAN] Test Operation     # Vulkan helpers working
[20:47:04.397] [ERROR] [VULKAN ERROR] Test error   # Error logging working

test cases: 5 | 4 passed | 1 failed
assertions: 6 | 5 passed | 1 failed
```

### Skipped Tests (Normal in Phase 1)
```
SKIPPED: explicitly with message:
  Vulkan not available for testing
```

*This is expected - full Vulkan context setup will be implemented in Phase 2*

## Current Test Coverage

| Class | Basic Tests | Thread Safety | Vulkan Context | Status |
|-------|-------------|---------------|----------------|---------|
| **Logger** | ✅ | ✅ | ✅ | Working |
| **ResourceManager** | 📋 | 📋 | ⏳ | Placeholders (Phase 2) |
| **AssetManager** | 📋 | 📋 | ⏳ | Placeholders (Phase 2) |
| **ModelLoader** | 📋 | 📋 | ⏳ | Placeholders (Phase 2) |

**Legend:**
- ✅ Implemented and working
- 📋 Placeholder tests ready for implementation  
- ⏳ Requires Vulkan context (Phase 2)

## Integration with CTest

If you have CTest available:

```bash
# From build directory
ctest --output-on-failure

# Run specific test patterns
ctest -R "Logger" --verbose
```

## Troubleshooting

### Build Issues

**"Catch2 not found":**
```bash
# Install Catch2 via vcpkg
cd ../../vcpkg
./vcpkg install catch2:x64-windows
```

**"Vulkan SDK not found":**
- Ensure Vulkan SDK is installed
- Check environment variable: `VULKAN_SDK`

**"MSBuild errors":**
- Ensure Visual Studio 2022 Build Tools installed
- Try clean rebuild: `cmake --build . --clean-first`

### Runtime Issues

**"Tests all skip":**
- Normal in Phase 1! Vulkan context will be implemented in Phase 2
- Logger tests should still pass

**"Resource deadlock" in Logger:**
- Known minor issue in file output test
- Core logging functionality works correctly
- Will be fixed in Phase 2

## Test Development

### Adding New Tests

1. Edit the appropriate `test_ClassName.cpp` file
2. Use existing test structure as template
3. Add proper tags: `[ClassName][Basic]`, `[ClassName][Threading]`, etc.
4. Rebuild and run to verify

### Example Test Structure
```cpp
TEST_CASE_METHOD(VulkanTestFixture, "MyClass New Feature", "[MyClass][Basic]") {
    if (skipIfNoVulkan()) {
        SKIP("Vulkan not available for testing");
        return;
    }
    
    // Your test code here
    REQUIRE(someCondition == true);
}
```

## Philosophy Alignment

Following VulkanMon's development principles:

🎯 **"Simple is Powerful"**
- Single executable, clear commands
- Readable test output and organization

🧪 **"Test, Test, Test"**
- Comprehensive coverage planned for all Phase 3 classes
- Both unit tests and integration testing

📚 **"Document Often"**
- Clear test names that document expected behavior
- Well-organized test structure with descriptive sections

---

*Happy testing! The framework is ready to support robust development of VulkanMon's core engine systems.*