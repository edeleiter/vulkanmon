#pragma once

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <string>
#include <functional>

/**
 * VulkanMon Window Management System
 * 
 * Handles GLFW window creation, surface management, and event callbacks.
 * Follows "Simple is Powerful" - focused solely on window lifecycle.
 * 
 * Responsibilities:
 * - GLFW window creation and management
 * - Vulkan surface creation
 * - Event callback routing
 * - Window state queries
 * 
 * Design Philosophy:
 * - Single responsibility: Window management only
 * - Clean interface: Clear callback registration
 * - RAII resource management: Automatic cleanup
 * - Testable: Can be mocked for unit testing
 */

namespace VulkanMon {

class Window {
public:
    // Callback type definitions for clean interface
    using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
    using MouseCallback = std::function<void(double xpos, double ypos)>;
    using ResizeCallback = std::function<void(int width, int height)>;

    /**
     * Create window with specified dimensions and title
     * 
     * @param width Window width in pixels
     * @param height Window height in pixels  
     * @param title Window title string
     */
    Window(uint32_t width, uint32_t height, const std::string& title);
    
    /**
     * Destructor - automatically cleans up GLFW resources
     */
    ~Window();
    
    // Move-only semantics (RAII compliance)
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = default;
    Window& operator=(Window&&) = default;
    
    /**
     * Initialize GLFW and create the window
     * Must be called before any other operations
     * 
     * @throws std::runtime_error if GLFW initialization or window creation fails
     */
    void initialize();
    
    /**
     * Create Vulkan surface for this window
     * Requires valid VkInstance
     * 
     * @param instance Valid Vulkan instance
     * @throws std::runtime_error if surface creation fails
     */
    void createSurface(VkInstance instance);
    
    /**
     * Check if window should close (user clicked X or pressed Alt+F4)
     * 
     * @return true if window should close
     */
    bool shouldClose() const;
    
    /**
     * Poll GLFW events - call once per frame in main loop
     */
    void pollEvents();
    
    /**
     * Manual cleanup - called automatically by destructor
     */
    void cleanup();
    
    // Callback registration
    /**
     * Register keyboard input callback
     * 
     * @param callback Function to call on key events
     */
    void setKeyCallback(KeyCallback callback);
    
    /**
     * Register mouse movement callback
     * 
     * @param callback Function to call on mouse movement
     */
    void setMouseCallback(MouseCallback callback);
    
    /**
     * Register window resize callback
     * 
     * @param callback Function to call on window resize
     */
    void setResizeCallback(ResizeCallback callback);
    
    // State queries (const interface)
    /**
     * Get raw GLFW window pointer
     * Used for direct GLFW operations
     * 
     * @return GLFW window pointer or nullptr if not initialized
     */
    GLFWwindow* getWindow() const { return window_; }
    
    /**
     * Get Vulkan surface handle
     * Valid after createSurface() call
     * 
     * @return Vulkan surface handle or VK_NULL_HANDLE if not created
     */
    VkSurfaceKHR getSurface() const { return surface_; }
    
    /**
     * Get window dimensions as Vulkan extent
     * 
     * @return VkExtent2D with current width/height
     */
    VkExtent2D getExtent() const { return {width_, height_}; }
    
    /**
     * Get window width
     * 
     * @return Current window width in pixels
     */
    uint32_t getWidth() const { return width_; }
    
    /**
     * Get window height
     * 
     * @return Current window height in pixels
     */
    uint32_t getHeight() const { return height_; }
    
    /**
     * Check if window was resized since last reset
     * 
     * @return true if window was resized
     */
    bool wasResized() const { return resized_; }
    
    /**
     * Reset the resize flag after handling resize
     */
    void resetResizeFlag() { resized_ = false; }
    
private:
    // Window state
    GLFWwindow* window_ = nullptr;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    uint32_t width_, height_;
    std::string title_;
    bool resized_ = false;
    
    // Callbacks
    KeyCallback keyCallback_;
    MouseCallback mouseCallback_;
    ResizeCallback resizeCallback_;
    
    // Static GLFW callbacks (bridge to member functions)
    static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfwMouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void glfwResizeCallback(GLFWwindow* window, int width, int height);
};

} // namespace VulkanMon