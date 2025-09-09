cmake_minimum_required(VERSION 3.20)
project(vulkanmon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Stb REQUIRED)

# Create executable
add_executable(vulkanmon
    src/main.cpp
    src/Camera.cpp
    src/Utils.cpp
)

# Link libraries
target_link_libraries(vulkanmon PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

# Include directories
target_include_directories(vulkanmon PRIVATE
    ${Vulkan_INCLUDE_DIRS}
    ${Stb_INCLUDE_DIR}
)

# Enable debug info
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(vulkanmon PRIVATE DEBUG)
endif()