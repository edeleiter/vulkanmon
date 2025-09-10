# VulkanMon C++ Unit Tests - Sweet Spot Approach
cmake_minimum_required(VERSION 3.20)
project(VulkanMonTests VERSION 0.1.0 LANGUAGES CXX)

# Use the same C++20 standard as the main project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages (minimal set for sweet spot approach)
find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# Include directories for VulkanMon source files
include_directories(../src)

# Create test executable - only core tests for sweet spot approach
add_executable(vulkanmon_tests
    main.cpp
    test_Logger.cpp
    test_Camera.cpp
    # test_ResourceManager.cpp  # Skip for now due to VulkanTestFixture complexity
    
    # Test fixtures and helpers
    fixtures/TestHelpers.cpp
    
    # Include only needed VulkanMon source files
    ../src/Logger.cpp
    ../src/Camera.cpp
    # ../src/ResourceManager.cpp  # Skip for now
)

# Link libraries - minimal set for sweet spot approach
target_link_libraries(vulkanmon_tests PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    Catch2::Catch2WithMain
)

# Include directories
target_include_directories(vulkanmon_tests PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    fixtures/
)

# Enable debug info for tests
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(vulkanmon_tests PRIVATE DEBUG)
endif()

# Discover and register tests with CTest
include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(vulkanmon_tests)

# Custom target for easy test running
add_custom_target(run_tests
    COMMAND vulkanmon_tests
    DEPENDS vulkanmon_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running VulkanMon unit tests (sweet spot approach)"
)