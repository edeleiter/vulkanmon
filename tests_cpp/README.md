# VulkanMon C++ Unit Testing Framework - Sweet Spot Implementation

## 🎯 **Overview**

This testing framework follows our **"Sweet Spot"** approach - maximum value with minimal complexity. We focus on testing the most critical components with the least setup overhead.

**Current Status**: **Framework Operational and Ready for Phase 4!**
- Logger Tests: 4/4 passing
- Camera Tests: 6/6 passing (edge case bugs FIXED!)
- Phase 4 Ready: Framework prepared for lighting and graphics testing
- Total Runtime: < 1 second

## Philosophy Alignment

Following VulkanMon's core development principles:

### 🎯 "Simple is Powerful"
- Clean test structure with readable assertions
- Focused test cases covering specific functionality
- Minimal setup required to run tests

### 🧪 "Test, Test, Test" 
- Comprehensive coverage of critical functionality
- Edge case testing and error condition validation
- Performance and thread safety testing

### 📚 "Document Often"
- Clear test names that document expected behavior
- Organized test suites with descriptive sections
- Test fixtures that demonstrate proper usage patterns

## Quick Start

### Prerequisites
- CMake 3.20+
- Vulkan SDK (for ResourceManager, AssetManager, ModelLoader tests)
- Catch2 3.x (installed via vcpkg)

### Building Tests
```bash
# From the tests_cpp directory
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

### Running Tests

**From project root:**
```bash
# Run all tests
cd build/tests_cpp/Debug
./vulkanmon_tests.exe

# Run specific test categories  
./vulkanmon_tests.exe "[Logger]"
./vulkanmon_tests.exe "[Camera]" 

# Run tests with specific tags
./vulkanmon_tests.exe "[Threading]"
./vulkanmon_tests.exe "[Basic]"
./vulkanmon_tests.exe "[Math]"
./vulkanmon_tests.exe "[Performance]"
```

**Alternative: From build/tests_cpp directory:**
```bash
cd build/tests_cpp
Debug/vulkanmon_tests.exe
Debug/vulkanmon_tests.exe "[Logger]"
Debug/vulkanmon_tests.exe "[Camera]"
```

### Running with CTest
```bash
# Discover and run tests via CTest
ctest --output-on-failure
ctest -R "Logger" --verbose
```

## Test Structure

### Test Organization
```
test_<ClassName>.cpp
├── Basic Functionality Tests
├── Advanced Feature Tests  
├── Error Handling Tests
├── Performance Tests
└── Thread Safety Tests
```

### Test Categories (Tags)
- `[Basic]` - Basic functionality tests
- `[Threading]` - Thread safety tests
- `[Performance]` - Performance and timing tests
- `[Vulkan]` - Tests requiring Vulkan context
- `[Errors]` - Error handling and edge cases
- `[RAII]` - Resource lifetime management tests

### Test Fixtures

#### VulkanTestFixture
Base class for tests requiring Vulkan context:
```cpp
class MyTestFixture : public VulkanTestFixture {
protected:
    void SetUp() {
        if (skipIfNoVulkan()) return;
        
        // Your setup code here
    }
};
```

#### LightweightTestFixture  
For tests that don't need Vulkan:
```cpp
class MyLightTest : public LightweightTestFixture {
    // Lightweight setup
};
```

## Test Implementation Status

### Phase 1: Framework Setup ✅
- [x] Catch2 integration
- [x] Test directory structure
- [x] CMake configuration
- [x] Test fixtures and helpers
- [x] Placeholder test files

### Phase 2: Core Tests (In Progress)
- [ ] Logger comprehensive tests
- [ ] ResourceManager RAII tests
- [ ] Basic integration testing

### Phase 3: Advanced Tests (Planned)
- [ ] AssetManager caching tests
- [ ] ModelLoader Assimp integration tests
- [ ] Performance benchmarking

### Phase 4: Polish (Planned)
- [ ] Test data generation
- [ ] CI/CD integration
- [ ] Coverage reporting

## Test Data

Test assets are located in `fixtures/test_assets/`:
- Sample texture files for AssetManager tests
- Simple OBJ models for ModelLoader tests
- Configuration files for various test scenarios

## Best Practices

### Writing New Tests
1. Use descriptive test names that document expected behavior
2. Group related tests in sections
3. Use appropriate test fixtures (Vulkan vs Lightweight)
4. Test both success and failure cases
5. Include performance tests for critical paths

### Test Categories
- Use tags to organize tests by functionality
- Follow the established naming patterns
- Test threading behavior for shared resources
- Validate RAII cleanup behavior

### Mocking Strategy
- VulkanTestFixture provides minimal Vulkan context
- Use dependency injection for testable designs
- Mock external dependencies (file system, etc.)
- Focus on testing our code, not external libraries

## Troubleshooting

### Common Issues

**Vulkan not available:**
- Tests will automatically skip with `SKIP("Vulkan not available")`
- Lightweight tests will still run

**Asset files missing:**
- Test helpers will create temporary assets
- Clean up is automatic via test fixtures

**Build failures:**
- Ensure vcpkg toolchain is configured
- Check that Catch2 and Vulkan SDK are installed

### Performance Considerations
- Tests should run quickly (< 10 seconds total)
- Use mocks for expensive operations
- Performance tests use reasonable iteration counts
- Thread safety tests use short timeouts

## Integration with Main Project

This C++ testing framework complements the existing Python pytest framework:

- **Python tests**: Integration testing, build validation, end-to-end testing
- **C++ tests**: Unit testing, class-level validation, performance testing

Both frameworks can be run independently and provide different levels of confidence in the system.

## Phase 4 Testing Expansion

### New Test Categories for Advanced Graphics
```bash
# Graphics and rendering tests
Debug/vulkanmon_tests.exe "[Lighting]"      # Lighting system tests
Debug/vulkanmon_tests.exe "[Materials]"     # Material system tests  
Debug/vulkanmon_tests.exe "[Shadows]"       # Shadow mapping tests
Debug/vulkanmon_tests.exe "[Performance]"   # Graphics performance tests
Debug/vulkanmon_tests.exe "[Visual]"        # Visual regression tests
```

### Phase 4 Testing Strategy
- **Lighting Math Tests**: Validate directional, point, and spot light calculations
- **Material Property Tests**: Test PBR material property calculations
- **Performance Regression Tests**: Ensure 60+ FPS with advanced graphics
- **Visual Validation Tests**: Screenshot comparison for rendering accuracy
- **Shader Compilation Tests**: Validate lighting/material shader compilation

### Planned Test Implementation
- **Step 32**: Add directional lighting calculation tests
- **Step 33**: Add point light attenuation tests  
- **Step 35**: Add PBR material property tests
- **Step 38**: Add shadow mapping validation tests
- **All Steps**: Performance impact measurement tests

---

*Following VulkanMon's development philosophy: "Simple is Powerful", "Test, Test, Test", "Document Often"*