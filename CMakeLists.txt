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
find_package(Jolt CONFIG REQUIRED)

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
    src/core/World.cpp
    src/rendering/VulkanRenderer.cpp
    src/core/Window.cpp
    src/core/InputHandler.cpp
    src/utils/Utils.cpp
    src/stb_image_impl.cpp
    src/rendering/ResourceManager.cpp
    src/utils/Logger.cpp
    src/io/AssetManager.cpp
    src/io/ModelLoader.cpp
    src/systems/LightingSystem.cpp
    src/systems/MaterialSystem.cpp
    src/systems/RenderSystem.cpp
    src/systems/CreatureRenderSystem.cpp
    src/systems/CameraSystem.cpp
    src/systems/CharacterControllerSystem.cpp
    src/systems/PhysicsSystem.cpp
    src/systems/ProjectileSystem.cpp
    src/materials/ProjectileMaterials.cpp
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
    Jolt::Jolt
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

# =============================================================================
# DOXYGEN DOCUMENTATION GENERATION
# =============================================================================

option(BUILD_DOCS "Generate API documentation with Doxygen" ON)

if(BUILD_DOCS)
    # Find Doxygen package
    find_package(Doxygen QUIET)

    if(DOXYGEN_FOUND)
        # Set Doxygen configuration variables
        set(DOXYGEN_PROJECT_NAME "VulkanMon Game Engine")
        set(DOXYGEN_PROJECT_NUMBER "7.1")
        set(DOXYGEN_PROJECT_BRIEF "Modern C++20 game engine with Vulkan graphics for Pokemon-style creature collector games")
        set(DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/docs")
        set(DOXYGEN_INPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/README.md")
        set(DOXYGEN_EXCLUDE_PATTERNS "*/build/* */vcpkg/* */.git/* */external/*")

        # Configure Doxygen settings
        set(DOXYGEN_GENERATE_HTML YES)
        set(DOXYGEN_GENERATE_LATEX NO)
        set(DOXYGEN_EXTRACT_ALL NO)
        set(DOXYGEN_EXTRACT_PRIVATE NO)
        set(DOXYGEN_EXTRACT_STATIC YES)
        set(DOXYGEN_HIDE_UNDOC_MEMBERS NO)
        set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
        set(DOXYGEN_SOURCE_BROWSER YES)
        set(DOXYGEN_ALPHABETICAL_INDEX YES)
        set(DOXYGEN_SEARCHENGINE YES)
        set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "README.md")
        set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
        set(DOXYGEN_WARN_IF_DOC_ERROR YES)
        set(DOXYGEN_WARN_NO_PARAMDOC YES)

        # Create documentation target
        doxygen_add_docs(docs
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/README.md
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating VulkanMon API documentation with Doxygen"
        )

        # Create a docs-open target for convenience (Windows)
        if(WIN32)
            add_custom_target(docs-open
                COMMAND cmd /c start ${CMAKE_CURRENT_BINARY_DIR}/docs/html/index.html
                DEPENDS docs
                COMMENT "Opening VulkanMon documentation in default browser"
            )
        endif()

        message(STATUS "Doxygen found: Documentation target 'docs' available")
        message(STATUS "  - Generate docs: cmake --build . --target docs")
        message(STATUS "  - Open docs (Windows): cmake --build . --target docs-open")
    else()
        message(WARNING "Doxygen not found: Documentation target disabled")
        message(STATUS "Install Doxygen to enable API documentation generation")
    endif()
endif()