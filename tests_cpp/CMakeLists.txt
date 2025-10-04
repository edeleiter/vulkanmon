# VulkanMon Unit Tests - Clean & Focused
cmake_minimum_required(VERSION 3.20)
project(VulkanMonTests VERSION 0.1.0 LANGUAGES CXX)

# Use the same C++20 standard as the main project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Coverage option (configuration applied after target creation)
option(ENABLE_COVERAGE "Enable test coverage reporting" OFF)

# Find packages
find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

# Include directories for VulkanMon source files
include_directories(../src)

# Create test executable - only for files that actually exist and compile
add_executable(vulkanmon_tests
    main.cpp
    
    # Core System Tests (working implementations)
    test_Logger.cpp
    # test_Camera.cpp  # Removed - old Camera class deleted
    test_Window.cpp
    test_InputHandler.cpp  # Re-enabled - simple integration approach
    
    # TODO: Application and VulkanRenderer require integration tests
    test_Application.cpp    # Re-enabled - fixed Config namespace references
    test_VulkanRenderer.cpp
    
    # Simple unit tests that work in isolation
    test_Utils.cpp
    test_ResourceManager.cpp
    test_LightingSystem.cpp
    
    # MaterialSystem tests (now properly isolated)
    test_MaterialSystem.cpp
    
    # Phase 2: Asset pipeline tests (now properly isolated)
    test_AssetManager.cpp
    test_ModelLoader.cpp

    # Phase 6: ECS Framework tests
    test_ecs.cpp
    test_ecs_integration.cpp  # Re-enabled - fixed compilation issues

    # Phase 6.3: ECS Inspector tests (NEW)
    test_ECSInspector.cpp

    # Phase 6.4: Window Resize tests (NEW)
    test_WindowResize.cpp

    # Spatial System tests (NEW)
    test_WorldConfig.cpp
    spatial/test_SpatialSystem.cpp
    spatial/test_SpatialPerformance.cpp
    spatial/bench_SpatialSystem.cpp

    # Physics System tests (Phase 7.1)
    physics/test_PhysicsComponents.cpp
    physics/test_PhysicsSystem.cpp
    physics/test_PhysicsBlockers.cpp

    # Performance Regression Tests (CRITICAL)
    performance/test_PerformanceRegression.cpp
    performance/test_CreatureDetectionRegression.cpp

    # Projectile System tests (Phase 8)
    test_ProjectileSystem.cpp

    # Debug Server tests (NEW)
    test_DebugServer.cpp
    test_FrameCapture.cpp

    # Test utilities
    fixtures/TestHelpers.cpp
    
    # Source files under test (individual components only)
    ../src/core/Window.cpp
    ../src/core/InputHandler.cpp  # Re-enabled with ECS integration
    ../src/core/Application.cpp  # Re-enabled for Application tests
    ../src/core/World.cpp  # Added for World::connectSystems() support
    # ../src/core/Camera.cpp  # Removed - old Camera class deleted
    ../src/utils/Utils.cpp
    ../src/stb_image_impl.cpp
    ../src/rendering/ResourceManager.cpp
    ../src/rendering/VulkanRenderer.cpp
    ../src/systems/LightingSystem.cpp
    ../src/systems/MaterialSystem.cpp
    ../src/systems/RenderSystem.cpp
    ../src/systems/CreatureRenderSystem.cpp  # Required by Application.cpp
    ../src/systems/CameraSystem.cpp
    ../src/systems/CharacterControllerSystem.cpp
    ../src/systems/PhysicsSystem.cpp
    ../src/systems/ProjectileSystem.cpp  # Required by Application.cpp
    ../src/materials/ProjectileMaterials.cpp  # Required by ProjectileSystem.cpp
    ../src/spatial/SpatialManager.cpp
    ../src/spatial/WorldConfig.cpp
    ../src/spatial/LayerMask.cpp
    ../src/spatial/SpatialCache.cpp
    ../src/io/AssetManager.cpp
    ../src/io/ModelLoader.cpp
    ../src/utils/Logger.cpp
    ../src/debug/ECSInspector.cpp
    ../src/debug/debug_server.cpp
    ../src/debug/frame_capture.cpp
)

# Link libraries
target_link_libraries(vulkanmon_tests PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    assimp::assimp
    Catch2::Catch2WithMain
    imgui::imgui
    Jolt::Jolt
    nlohmann_json::nlohmann_json
    httplib::httplib
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

# Simple coverage support - following "Simple is Powerful" philosophy
# NOTE: Applied after target creation to avoid CMake configuration errors
if(ENABLE_COVERAGE)
    if(MSVC)
        # MSVC coverage requires Visual Studio Enterprise or Professional
        # For now, just enable debug info for analysis
        message(STATUS "Coverage build enabled - use Visual Studio coverage tools")
        target_compile_options(vulkanmon_tests PRIVATE /Zi)
        target_link_options(vulkanmon_tests PRIVATE /DEBUG)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Coverage enabled for GCC/Clang")
        target_compile_options(vulkanmon_tests PRIVATE --coverage)
        target_link_options(vulkanmon_tests PRIVATE --coverage)
    endif()
endif()

# Custom target for easy test running
add_custom_target(run_tests
    COMMAND vulkanmon_tests
    DEPENDS vulkanmon_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running VulkanMon focused unit tests"
)