cmake_minimum_required(VERSION 3.20)

# Set vcpkg toolchain if not already set
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(vulkanmon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages (vcpkg will handle these automatically)
find_package(Vulkan REQUIRED COMPONENTS glslangValidator)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

# Create executable
# Use CMake's Vulkan component for glslangValidator
if(NOT Vulkan_glslangValidator_FOUND)
    message(FATAL_ERROR "glslangValidator not found! Make sure Vulkan SDK is installed or vcpkg glslang package is available.")
endif()
set(GLSL_VALIDATOR ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE})

# Function to compile shaders
function(compile_shader SOURCE_FILE TARGET_FILE)
    add_custom_command(
        OUTPUT ${TARGET_FILE}
        COMMAND ${GLSL_VALIDATOR} -V ${SOURCE_FILE} -o ${TARGET_FILE}
        DEPENDS ${SOURCE_FILE}
        COMMENT "Compiling shader ${SOURCE_FILE}"
        VERBATIM
    )
endfunction()

# Compile shaders
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
compile_shader("${SHADER_DIR}/triangle.vert" "${SHADER_DIR}/vert.spv")
compile_shader("${SHADER_DIR}/triangle.frag" "${SHADER_DIR}/frag.spv")

# Create a custom target for shaders
add_custom_target(shaders 
    DEPENDS 
        "${SHADER_DIR}/vert.spv" 
        "${SHADER_DIR}/frag.spv"
)

add_executable(vulkanmon
    src/main.cpp
    src/core/Application.cpp
    src/rendering/VulkanRenderer.cpp
    src/core/Window.cpp
    src/core/InputHandler.cpp
    src/core/Camera.cpp
    src/utils/Utils.cpp
    src/stb_image_impl.cpp
    src/rendering/ResourceManager.cpp
    src/utils/Logger.cpp
    src/io/AssetManager.cpp
    src/io/ModelLoader.cpp
    src/systems/LightingSystem.cpp
    src/systems/MaterialSystem.cpp
    src/systems/RenderSystem.cpp
    src/systems/CameraSystem.cpp
    src/debug/ECSInspector.cpp
    src/spatial/SpatialManager.cpp
    src/spatial/WorldConfig.cpp
    src/spatial/LayerMask.cpp
    src/spatial/SpatialCache.cpp
)

# Make sure shaders are compiled before building the executable
add_dependencies(vulkanmon shaders)

# Link libraries
target_link_libraries(vulkanmon PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    assimp::assimp
    imgui::imgui
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

# Add tests subdirectory (optional - can be disabled for release builds)
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_subdirectory(tests_cpp)
endif()