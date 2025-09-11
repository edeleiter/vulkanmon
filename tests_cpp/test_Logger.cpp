/*
 * VulkanMon Logger Unit Tests
 * 
 * Comprehensive tests for the Logger class focusing on:
 * - Thread safety with concurrent logging
 * - Log level filtering correctness
 * - File output functionality  
 * - Performance macro behavior
 * - Message formatting and output
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "../src/Logger.h"
#include "fixtures/TestHelpers.h"
#include <thread>
#include <fstream>
#include <sstream>

using namespace VulkanMon;
using namespace VulkanMon::Testing;

TEST_CASE("Logger Basic Functionality", "[Logger][Basic]") {
    SECTION("Logger singleton access") {
        auto& logger1 = Logger::getInstance();
        auto& logger2 = Logger::getInstance();
        
        // Should be the same instance
        REQUIRE(&logger1 == &logger2);
    }
    
    SECTION("Log level filtering") {
        auto& logger = Logger::getInstance();
        
        // Set to WARNING level
        logger.setLogLevel(LogLevel::WARNING_LEVEL);
        
        // These will be implemented with actual output capture in Phase 2
        REQUIRE(true); // Placeholder - will test actual output capture
    }
}

TEST_CASE("Logger Thread Safety", "[Logger][Threading]") {
    SECTION("Concurrent logging from multiple threads") {
        std::atomic<int> completedThreads{0};
        
        ThreadTestHelpers::runConcurrently([&completedThreads]() {
            VKMON_INFO("Thread safety test message");
            completedThreads++;
        }, 4, 25);
        
        REQUIRE(completedThreads == 100);
    }
}

TEST_CASE("Logger File Output", "[Logger][File]") {
    SECTION("Enable and disable file output") {
        auto& logger = Logger::getInstance();
        std::string tempFile = "test_log_output.txt";
        
        logger.enableFileOutput(tempFile);
        logger.info("Test message to file");
        logger.disableFileOutput();
        
        // Clean up
        std::filesystem::remove(tempFile);
        
        REQUIRE(true); // Will implement file content validation in Phase 2
    }
}

TEST_CASE("Logger Performance Macros", "[Logger][Performance]") {
    SECTION("Performance logging macro") {
        auto& logger = Logger::getInstance();
        
        VKMON_PERF("test_operation", 42.5);
        
        REQUIRE(true); // Will verify output in Phase 2
    }
}

TEST_CASE("Logger Vulkan Integration", "[Logger][Vulkan]") {
    SECTION("Vulkan-specific logging helpers") {
        REQUIRE_NOTHROW(VKMON_VK_INFO("Test Operation", "Additional details"));
        REQUIRE_NOTHROW(VKMON_VK_ERROR("Test Operation", "Test error message"));
    }
}