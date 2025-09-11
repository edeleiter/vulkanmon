#define GLFW_INCLUDE_VULKAN
#include "Window.h"
#include "Logger.h"
#include <stdexcept>

namespace VulkanMon {

Window::Window(uint32_t width, uint32_t height, const std::string& title)
    : width_(width), height_(height), title_(title) {
    VKMON_DEBUG("Window object created: " + std::to_string(width) + "x" + std::to_string(height) + " '" + title + "'");
}

Window::~Window() {
    cleanup();
}

void Window::initialize() {
    VKMON_INFO("Initializing GLFW window system...");
    
    // Initialize GLFW
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }
    
    // Configure GLFW for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    
    // Create the window
    window_ = glfwCreateWindow(static_cast<int>(width_), static_cast<int>(height_), 
                              title_.c_str(), nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }
    
    // Set user pointer for callbacks
    glfwSetWindowUserPointer(window_, this);
    
    // Register static GLFW callbacks
    glfwSetKeyCallback(window_, glfwKeyCallback);
    glfwSetCursorPosCallback(window_, glfwMouseCallback);
    glfwSetFramebufferSizeCallback(window_, glfwResizeCallback);
    
    // Configure mouse input for 3D camera controls
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    VKMON_INFO("GLFW window initialized successfully");
    VKMON_RESOURCE("Window", "created", title_ + " (" + std::to_string(width_) + "x" + std::to_string(height_) + ")");
}

void Window::createSurface(VkInstance instance) {
    VKMON_INFO("Creating Vulkan surface for window...");
    
    if (!window_) {
        throw std::runtime_error("Cannot create surface: window not initialized");
    }
    
    VkResult result = glfwCreateWindowSurface(instance, window_, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface (VkResult: " + std::to_string(result) + ")");
    }
    
    VKMON_INFO("Vulkan surface created successfully");
    VKMON_RESOURCE("Surface", "created", "VkSurfaceKHR for " + title_);
}

bool Window::shouldClose() const {
    return window_ ? glfwWindowShouldClose(window_) : true;
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::cleanup() {
    if (surface_ != VK_NULL_HANDLE) {
        // Note: Surface cleanup is handled by VulkanRenderer
        // since it needs the VkInstance that created it
        surface_ = VK_NULL_HANDLE;
    }
    
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        VKMON_DEBUG("GLFW window destroyed");
    }
    
    // Only terminate GLFW if we initialized it
    // In a real application, this might be managed by a higher-level system
    static bool glfwTerminated = false;
    if (!glfwTerminated) {
        glfwTerminate();
        glfwTerminated = true;
        VKMON_DEBUG("GLFW terminated");
    }
}

// Callback registration
void Window::setKeyCallback(KeyCallback callback) {
    keyCallback_ = callback;
    VKMON_DEBUG("Key callback registered");
}

void Window::setMouseCallback(MouseCallback callback) {
    mouseCallback_ = callback;
    VKMON_DEBUG("Mouse callback registered");
}

void Window::setResizeCallback(ResizeCallback callback) {
    resizeCallback_ = callback;
    VKMON_DEBUG("Resize callback registered");
}

// Static GLFW callback bridges
// These functions receive GLFW events and route them to the appropriate Window instance

void Window::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowObj && windowObj->keyCallback_) {
        windowObj->keyCallback_(key, scancode, action, mods);
    }
}

void Window::glfwMouseCallback(GLFWwindow* window, double xpos, double ypos) {
    Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowObj && windowObj->mouseCallback_) {
        windowObj->mouseCallback_(xpos, ypos);
    }
}

void Window::glfwResizeCallback(GLFWwindow* window, int width, int height) {
    Window* windowObj = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (windowObj) {
        // Update internal state
        windowObj->width_ = static_cast<uint32_t>(width);
        windowObj->height_ = static_cast<uint32_t>(height);
        windowObj->resized_ = true;
        
        VKMON_DEBUG("Window resized to " + std::to_string(width) + "x" + std::to_string(height));
        
        // Notify callback if registered
        if (windowObj->resizeCallback_) {
            windowObj->resizeCallback_(width, height);
        }
    }
}

} // namespace VulkanMon