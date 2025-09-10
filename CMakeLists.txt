cmake_minimum_required(VERSION 3.20)
project(vulkanmon VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Stb REQUIRED)
find_package(assimp REQUIRED)

# Create executable
# Find glslangValidator for shader compilation
find_program(GLSL_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/bin)
if(NOT GLSL_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found! Make sure Vulkan SDK is installed.")
endif()

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
    src/Camera.cpp
    src/Utils.cpp
    src/ResourceManager.cpp
    src/Logger.cpp
    src/AssetManager.cpp
    src/ModelLoader.cpp
    src/LightingSystem.cpp
    src/MaterialSystem.cpp
)

# Make sure shaders are compiled before building the executable
add_dependencies(vulkanmon shaders)

# Link libraries
target_link_libraries(vulkanmon PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    assimp::assimp
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