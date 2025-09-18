/**
 * VulkanMon InputHandler Unit Tests
 *
 * Testing input processing and system control - focused on our new InputHandler class
 * Simple integration approach - tests basic functionality without complex ECS mocking
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/core/InputHandler.h"
#include "../src/core/World.h"
#include "../src/core/Window.h"
#include "fixtures/TestHelpers.h"
#include <GLFW/glfw3.h>
#include <memory>

using namespace VulkanMon::Testing;
using Catch::Approx;

// Mock Window for testing
class MockWindow : public VulkanMon::Window {
public:
    MockWindow() : VulkanMon::Window(800, 600, "MockWindow") {}

    mutable bool cursorDisabled = true;  // Start with cursor disabled (camera mode)
    mutable int enableCursorCallCount = 0;
    mutable int disableCursorCallCount = 0;

    void enableCursor() override {
        cursorDisabled = false;
        enableCursorCallCount++;
    }

    void disableCursor() override {
        cursorDisabled = true;
        disableCursorCallCount++;
    }

    bool isCursorDisabled() const override {
        return cursorDisabled;
    }

    // Override initialize to do nothing for tests (no actual GLFW)
    void initialize() override {
        // Do nothing for tests
    }
};

// Test fixture for GLFW initialization (needed for key constants)
class InputTestFixture {
public:
    InputTestFixture() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW for testing");
        }
    }

    ~InputTestFixture() {
        glfwTerminate();
    }

    // Simple helper to create InputHandler with minimal ECS setup
    std::unique_ptr<VulkanMon::InputHandler> createInputHandler() {
        // Create minimal ECS world
        world = std::make_unique<VulkanMon::World>();
        cameraSystem = world->addSystem<VulkanMon::CameraSystem>();
        world->initialize();

        // Create mock window
        window = std::make_shared<MockWindow>();

        // Create InputHandler with real ECS components
        return std::make_unique<VulkanMon::InputHandler>(window, cameraSystem, world.get());
    }

private:
    std::unique_ptr<VulkanMon::World> world;
    VulkanMon::CameraSystem* cameraSystem = nullptr;
    std::shared_ptr<MockWindow> window;
};

TEST_CASE("InputHandler Basic Construction", "[InputHandler][Basic]") {
    InputTestFixture fixture;

    SECTION("Construction with minimal ECS setup") {
        auto inputHandler = fixture.createInputHandler();

        // Should construct successfully
        REQUIRE(inputHandler != nullptr);
    }

    SECTION("Default configuration values") {
        auto inputHandler = fixture.createInputHandler();

        // Mouse should be locked by default
        REQUIRE(inputHandler->isMouseLocked());
    }
}

TEST_CASE("InputHandler Configuration", "[InputHandler][Configuration]") {
    InputTestFixture fixture;

    SECTION("Mouse sensitivity configuration") {
        auto inputHandler = fixture.createInputHandler();

        REQUIRE_NOTHROW(inputHandler->setMouseSensitivity(0.5f));
        REQUIRE_NOTHROW(inputHandler->setMouseSensitivity(2.0f));
        REQUIRE_NOTHROW(inputHandler->setMouseSensitivity(0.0f)); // Edge case
    }

    SECTION("Camera speed configuration") {
        auto inputHandler = fixture.createInputHandler();

        REQUIRE_NOTHROW(inputHandler->setCameraSpeed(1.0f));
        REQUIRE_NOTHROW(inputHandler->setCameraSpeed(10.0f));
        REQUIRE_NOTHROW(inputHandler->setCameraSpeed(0.0f)); // Edge case
    }

    SECTION("Mouse lock state") {
        auto inputHandler = fixture.createInputHandler();

        REQUIRE(inputHandler->isMouseLocked());

        inputHandler->setMouseLocked(false);
        REQUIRE_FALSE(inputHandler->isMouseLocked());

        inputHandler->setMouseLocked(true);
        REQUIRE(inputHandler->isMouseLocked());
    }

    SECTION("Mouse position reset") {
        auto inputHandler = fixture.createInputHandler();

        REQUIRE_NOTHROW(inputHandler->resetMousePosition());
    }
}

TEST_CASE("InputHandler Key Input Processing", "[InputHandler][KeyInput]") {
    InputTestFixture fixture;

    SECTION("System control keys") {
        auto inputHandler = fixture.createInputHandler();

        bool shaderReloadCalled = false;
        inputHandler->setShaderReloadCallback([&]() {
            shaderReloadCalled = true;
        });

        // Test R key (shader reload)
        inputHandler->processKeyInput(GLFW_KEY_R, 0, GLFW_PRESS, 0);
        REQUIRE(shaderReloadCalled);

        // Test key release should not trigger
        shaderReloadCalled = false;
        inputHandler->processKeyInput(GLFW_KEY_R, 0, GLFW_RELEASE, 0);
        REQUIRE_FALSE(shaderReloadCalled);
    }

    SECTION("Unhandled keys") {
        auto inputHandler = fixture.createInputHandler();

        // Unhandled keys should not crash
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_Z, 0, GLFW_PRESS, 0));
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_F1, 0, GLFW_PRESS, 0));
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0));
    }
}

TEST_CASE("InputHandler Mouse Input Processing", "[InputHandler][MouseInput]") {
    InputTestFixture fixture;

    SECTION("Mouse input with locked mouse") {
        auto inputHandler = fixture.createInputHandler();

        inputHandler->setMouseLocked(true);

        // Should handle mouse movement without crashing
        REQUIRE_NOTHROW(inputHandler->processMouseInput(400.0, 300.0));
        REQUIRE_NOTHROW(inputHandler->processMouseInput(450.0, 350.0));
    }

    SECTION("Mouse input with unlocked mouse") {
        auto inputHandler = fixture.createInputHandler();

        inputHandler->setMouseLocked(false);

        // Should handle gracefully when mouse is not locked
        REQUIRE_NOTHROW(inputHandler->processMouseInput(400.0, 300.0));
    }
}

TEST_CASE("InputHandler Callback Management", "[InputHandler][Callbacks]") {
    InputTestFixture fixture;

    SECTION("Setting callbacks before use") {
        auto inputHandler = fixture.createInputHandler();

        // Set callbacks
        bool shaderReloadCalled = false;
        bool lightingControlCalled = false;
        bool materialControlCalled = false;

        inputHandler->setShaderReloadCallback([&]() { shaderReloadCalled = true; });
        inputHandler->setLightingControlCallback([&](int) { lightingControlCalled = true; });
        inputHandler->setMaterialControlCallback([&](int) { materialControlCalled = true; });

        // Trigger callbacks
        inputHandler->processKeyInput(GLFW_KEY_R, 0, GLFW_PRESS, 0);
        inputHandler->processKeyInput(GLFW_KEY_1, 0, GLFW_PRESS, 0);
        inputHandler->processKeyInput(GLFW_KEY_M, 0, GLFW_PRESS, 0);

        REQUIRE(shaderReloadCalled);
        REQUIRE(lightingControlCalled);
        REQUIRE(materialControlCalled);
    }

    SECTION("Using input without callbacks registered") {
        auto inputHandler = fixture.createInputHandler();

        // Should not crash when callbacks are not set
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_R, 0, GLFW_PRESS, 0));
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_1, 0, GLFW_PRESS, 0));
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_M, 0, GLFW_PRESS, 0));
    }
}

TEST_CASE("InputHandler Error Handling", "[InputHandler][ErrorHandling]") {
    InputTestFixture fixture;

    SECTION("Invalid key codes") {
        auto inputHandler = fixture.createInputHandler();

        // Should handle invalid key codes gracefully
        REQUIRE_NOTHROW(inputHandler->processKeyInput(-1, 0, GLFW_PRESS, 0));
        REQUIRE_NOTHROW(inputHandler->processKeyInput(999999, 0, GLFW_PRESS, 0));
    }

    SECTION("Invalid action codes") {
        auto inputHandler = fixture.createInputHandler();

        // Should handle invalid action codes gracefully
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_A, 0, -1, 0));
        REQUIRE_NOTHROW(inputHandler->processKeyInput(GLFW_KEY_A, 0, 999, 0));
    }
}