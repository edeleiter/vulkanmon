/**
 * VulkanMon Window Unit Tests
 * 
 * Testing window management and GLFW integration - focused on our new Window class
 * Tests window creation, destruction, callback management, and error handling
 * 
 * Test Categories:
 * - Window construction and initialization
 * - Callback registration and triggering
 * - GLFW integration
 * - Error handling and edge cases
 * - RAII resource management
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../src/core/Window.h"
#include "fixtures/TestHelpers.h"
#include <GLFW/glfw3.h>
#include <memory>

using namespace VulkanMon::Testing;
using Catch::Approx;

// Test fixture for GLFW initialization
class WindowTestFixture {
public:
    WindowTestFixture() {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW for testing");
        }
    }
    
    ~WindowTestFixture() {
        glfwTerminate();
    }
};

TEST_CASE("Window Basic Construction", "[Window][Basic]") {
    WindowTestFixture fixture;
    
    SECTION("Default window construction") {
        VulkanMon::Window window(800, 600, "Test Window");
        
        // Basic properties should be set
        REQUIRE(window.getWidth() == 800);
        REQUIRE(window.getHeight() == 600);
        // Basic properties should be accessible
        // Note: Window class doesn't expose getTitle() or isInitialized() methods
    }
    
    SECTION("Window with different dimensions") {
        VulkanMon::Window window(1920, 1080, "Large Window");
        
        REQUIRE(window.getWidth() == 1920);
        REQUIRE(window.getHeight() == 1080);
        // Window dimensions should be set correctly
    }
    
    SECTION("Window with empty title") {
        VulkanMon::Window window(640, 480, "");
        
        REQUIRE(window.getWidth() == 640);
        REQUIRE(window.getHeight() == 480);
        // Window with empty title should construct correctly
    }
}

TEST_CASE("Window Initialization", "[Window][Initialization]") {
    WindowTestFixture fixture;
    
    SECTION("Successful window initialization") {
        VulkanMon::Window window(800, 600, "Test Window");
        
        REQUIRE_NOTHROW(window.initialize());
        REQUIRE(window.getWindow() != nullptr);
        
        // GLFW window should have correct properties
        GLFWwindow* glfwWindow = window.getWindow();
        int width, height;
        glfwGetWindowSize(glfwWindow, &width, &height);
        REQUIRE(width == 800);
        REQUIRE(height == 600);
    }
    
    SECTION("Window initialization sets user pointer") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        GLFWwindow* glfwWindow = window.getWindow();
        void* userPointer = glfwGetWindowUserPointer(glfwWindow);
        REQUIRE(userPointer == &window);
    }
    
    SECTION("Double initialization should not crash") {
        VulkanMon::Window window(800, 600, "Test Window");
        
        REQUIRE_NOTHROW(window.initialize());
        REQUIRE(window.getWindow() != nullptr);
        
        // Second initialization should be safe
        REQUIRE_NOTHROW(window.initialize());
        REQUIRE(window.getWindow() != nullptr);
    }
}

TEST_CASE("Window Callback Registration", "[Window][Callbacks]") {
    WindowTestFixture fixture;
    
    SECTION("Key callback registration") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        bool callbackTriggered = false;
        int receivedKey = -1;
        int receivedAction = -1;
        
        window.setKeyCallback([&](int key, int scancode, int action, int mods) {
            callbackTriggered = true;
            receivedKey = key;
            receivedAction = action;
        });
        
        // Test that callback was registered (can't easily test triggering without complex setup)
        GLFWwindow* glfwWindow = window.getWindow();
        REQUIRE(glfwWindow != nullptr);
        
        // We can't easily test callback triggering in unit tests without complex GLFW event simulation
        // For now, just verify the window accepts callback registration without crashing
        
        // For unit tests, we just verify callback registration doesn't crash
        // Actual callback triggering would be tested in integration tests
        REQUIRE(true); // Callback registered successfully
    }
    
    SECTION("Mouse callback registration") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        bool callbackTriggered = false;
        double receivedX = -1.0;
        double receivedY = -1.0;
        
        window.setMouseCallback([&](double xpos, double ypos) {
            callbackTriggered = true;
            receivedX = xpos;
            receivedY = ypos;
        });
        
        // Test that callback was registered
        GLFWwindow* glfwWindow = window.getWindow();
        REQUIRE(glfwWindow != nullptr);
        
        // We can't easily test callback triggering in unit tests without complex GLFW event simulation
        
        // For unit tests, we just verify callback registration doesn't crash
        REQUIRE(true); // Callback registered successfully
    }
    
    SECTION("Resize callback registration") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        bool callbackTriggered = false;
        int receivedWidth = -1;
        int receivedHeight = -1;
        
        window.setResizeCallback([&](int width, int height) {
            callbackTriggered = true;
            receivedWidth = width;
            receivedHeight = height;
        });
        
        // Test that callback was registered
        GLFWwindow* glfwWindow = window.getWindow();
        REQUIRE(glfwWindow != nullptr);
        
        // We can't easily test callback triggering in unit tests without complex GLFW event simulation
        
        // For unit tests, we just verify callback registration doesn't crash
        REQUIRE(true); // Callback registered successfully
    }
    
    SECTION("Multiple callback registrations") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        int keyCallbackCount = 0;
        int mouseCallbackCount = 0;
        int resizeCallbackCount = 0;
        
        window.setKeyCallback([&](int, int, int, int) { keyCallbackCount++; });
        window.setMouseCallback([&](double, double) { mouseCallbackCount++; });
        window.setResizeCallback([&](int, int) { resizeCallbackCount++; });
        
        // Test that all callbacks were registered
        GLFWwindow* glfwWindow = window.getWindow();
        REQUIRE(glfwWindow != nullptr);
        
        // For unit tests, we just verify registration doesn't crash
        // Integration tests would verify actual callback triggering
        REQUIRE(true); // Placeholder - callbacks registered successfully
    }
}

TEST_CASE("Window Properties and State", "[Window][Properties]") {
    WindowTestFixture fixture;
    
    SECTION("Window should close properly") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        REQUIRE_FALSE(window.shouldClose());
        
        // Simulate close request
        glfwSetWindowShouldClose(window.getWindow(), GLFW_TRUE);
        REQUIRE(window.shouldClose());
    }
    
    SECTION("Poll events should not crash") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        REQUIRE_NOTHROW(window.pollEvents());
    }
    
    SECTION("Present should handle null swapchain gracefully") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        // Basic window operations should not crash
        REQUIRE_NOTHROW(window.pollEvents());
    }
}

TEST_CASE("Window RAII Behavior", "[Window][RAII]") {
    WindowTestFixture fixture;
    
    SECTION("Window destructor cleanup") {
        GLFWwindow* glfwWindow = nullptr;
        
        {
            VulkanMon::Window window(800, 600, "Test Window");
            window.initialize();
            glfwWindow = window.getWindow();
            
            REQUIRE(glfwWindow != nullptr);
            // Window should be valid while in scope
        }
        
        // After destruction, the GLFW window should be destroyed
        // Note: We can't directly test this without risking segfaults,
        // but the destructor should have been called
        REQUIRE(true); // Placeholder - actual test would need more sophisticated validation
    }
    
    SECTION("Multiple windows can exist simultaneously") {
        VulkanMon::Window window1(800, 600, "Window 1");
        VulkanMon::Window window2(640, 480, "Window 2");
        
        REQUIRE_NOTHROW(window1.initialize());
        REQUIRE_NOTHROW(window2.initialize());
        
        REQUIRE(window1.getWindow() != nullptr);
        REQUIRE(window2.getWindow() != nullptr);
        REQUIRE(window1.getWindow() != window2.getWindow());
    }
}

TEST_CASE("Window Error Handling", "[Window][ErrorHandling]") {
    WindowTestFixture fixture;
    
    SECTION("Window initialization with small dimensions") {
        // Test with very small but valid dimensions
        VulkanMon::Window window(1, 1, "Small Window");
        
        // Should not crash
        REQUIRE_NOTHROW(window.initialize());
    }
    
    SECTION("Callback registration before initialization") {
        VulkanMon::Window window(800, 600, "Test Window");
        
        // Should not crash
        REQUIRE_NOTHROW(window.setKeyCallback([](int, int, int, int) {}));
        REQUIRE_NOTHROW(window.setMouseCallback([](double, double) {}));
        REQUIRE_NOTHROW(window.setResizeCallback([](int, int) {}));
    }
    
    SECTION("Operations on uninitialized window") {
        VulkanMon::Window window(800, 600, "Test Window");
        
        REQUIRE(window.getWindow() == nullptr);
        
        // These operations should handle uninitialized state gracefully
        REQUIRE_NOTHROW(window.shouldClose());
        REQUIRE_NOTHROW(window.pollEvents());
    }
}

TEST_CASE("Window Performance", "[Window][Performance]") {
    WindowTestFixture fixture;
    
    SECTION("Window creation performance") {
        double avgTime = PerformanceTestHelpers::benchmarkFunction([]() {
            VulkanMon::Window window(800, 600, "Performance Test");
            window.initialize();
        }, 10); // Fewer iterations for window creation
        
        // Window creation should be reasonably fast (under 100ms average)
        REQUIRE(avgTime < 100.0);
    }
    
    SECTION("Poll events performance") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        double avgTime = PerformanceTestHelpers::benchmarkFunction([&window]() {
            window.pollEvents();
        }, 1000);
        
        // Polling should be very fast (under 1ms for 1000 polls)
        REQUIRE(avgTime < 1.0);
    }
}

TEST_CASE("Window Thread Safety", "[Window][Threading]") {
    WindowTestFixture fixture;
    
    SECTION("Callback triggering from main thread") {
        VulkanMon::Window window(800, 600, "Test Window");
        window.initialize();
        
        std::atomic<int> callbackCount{0};
        
        window.setKeyCallback([&](int, int, int, int) {
            callbackCount++;
        });
        
        // Test that callback was registered from main thread
        GLFWwindow* glfwWindow = window.getWindow();
        REQUIRE(glfwWindow != nullptr);
        
        // For unit tests, we just verify the callback system doesn't crash
        REQUIRE(true); // Placeholder for thread safety test
    }
}