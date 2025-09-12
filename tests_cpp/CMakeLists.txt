# VulkanMon Unit Tests - Clean & Focused
cmake_minimum_required(VERSION 3.20)
project(VulkanMonTests VERSION 0.1.0 LANGUAGES CXX)

# Use the same C++20 standard as the main project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Stb REQUIRED)

# Include directories for VulkanMon source files
include_directories(../src)

# Create test executable - only for files that actually exist and compile
add_executable(vulkanmon_tests
    main.cpp
    
    # Core System Tests (working implementations)
    test_Logger.cpp
    test_Camera.cpp
    test_Window.cpp
    test_InputHandler.cpp
    
    # Test utilities
    fixtures/TestHelpers.cpp
    
    # Source files under test (individual components only)
    ../src/core/Window.cpp
    ../src/core/InputHandler.cpp
    ../src/core/Camera.cpp
    ../src/utils/Utils.cpp
    ../src/stb_image_impl.cpp
    ../src/rendering/ResourceManager.cpp
    ../src/utils/Logger.cpp
)

# Link libraries
target_link_libraries(vulkanmon_tests PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    assimp::assimp
    Catch2::Catch2WithMain
)

# Include directories
target_include_directories(vulkanmon_tests PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${Stb_INCLUDE_DIR}
    fixtures/
)

# Enable debug info for tests
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(vulkanmon_tests PRIVATE DEBUG)
endif()

# Custom target for easy test running
add_custom_target(run_tests
    COMMAND vulkanmon_tests
    DEPENDS vulkanmon_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running VulkanMon focused unit tests"
)