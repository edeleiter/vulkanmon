/*
 * VulkanMon - 3D Graphics Engine
 * 
 * Modern C++20 game engine with Vulkan graphics, designed as a foundation 
 * for Pokemon-style 3D creature collector games.
 * 
 * Phase 5.2: Refactored Architecture
 * - Clean separation of concerns
 * - Application class orchestrates all systems
 * - VulkanRenderer handles all rendering
 * - Testable and maintainable codebase
 * 
 * Core Principles:
 * - "Simple is Powerful": Clean, focused interfaces
 * - "Test, Test, Test": Comprehensive unit testing
 * - "Document Often": Self-documenting code
 */

#include "Application.h"
#include "Logger.h"

#include <iostream>
#include <exception>
#include <cstdlib>

using namespace VulkanMon;

int main() {
    // Initialize minimal logging for startup
    Logger::getInstance().enableConsoleOutput(true);
    Logger::getInstance().setLogLevel(LogLevel::INFO_LEVEL);
    
    VKMON_INFO("=== VulkanMon Engine Starting ===");
    VKMON_INFO("Phase 5.2: Refactored Architecture");
    VKMON_INFO("Modern C++20 Vulkan Graphics Engine");
    
    try {
        // Create and initialize application
        Application app;
        VKMON_INFO("Application created successfully");
        
        // Initialize all engine systems
        app.initialize();
        VKMON_INFO("Application initialized successfully");
        
        // Run main application loop
        VKMON_INFO("Starting main application loop...");
        app.run();
        
        // Clean shutdown
        VKMON_INFO("Application finished successfully");
        
    } catch (const std::exception& e) {
        // Handle any critical errors
        VKMON_ERROR("Critical application error: " + std::string(e.what()));
        std::cerr << "\nVulkanMon encountered a critical error:\n";
        std::cerr << e.what() << std::endl;
        std::cerr << "\nPlease check the console output above for detailed error information.\n";
        return EXIT_FAILURE;
    } catch (...) {
        // Handle unexpected errors
        VKMON_ERROR("Unknown critical error occurred");
        std::cerr << "\nVulkanMon encountered an unknown error.\n";
        std::cerr << "Please check the console output for any error information.\n";
        return EXIT_FAILURE;
    }
    
    VKMON_INFO("=== VulkanMon Engine Shutdown Complete ===");
    return EXIT_SUCCESS;
}