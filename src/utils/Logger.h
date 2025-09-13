#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <memory>
#include <mutex>

/**
 * VulkanMon Logging System
 * 
 * Simple, thread-safe logging following our core philosophy:
 * - "Simple is Powerful" - Easy to use, clear output, minimal setup
 * - "Test, Test, Test" - Reliable logging for debugging and validation
 * - "Document Often" - Clear log levels and structured output
 * 
 * Features:
 * - Multiple log levels (Debug, Info, Warning, Error, Fatal)
 * - Console and file output
 * - Thread-safe logging
 * - Automatic timestamping
 * - Configurable output formatting
 * - Performance-friendly (minimal overhead in release builds)
 */

namespace VulkanMon {

/**
 * Log levels in order of severity
 */
enum class LogLevel {
    DEBUG_LEVEL = 0,   // Development debugging information
    INFO_LEVEL = 1,    // General information messages
    WARNING_LEVEL = 2, // Warning messages (recoverable issues)
    ERROR_LEVEL = 3,   // Error messages (serious problems)
    FATAL_LEVEL = 4    // Fatal errors (application will terminate)
};

/**
 * Singleton Logger class
 * Provides centralized logging with thread-safety and multiple outputs
 */
class Logger {
public:
    // Get singleton instance
    static Logger& getInstance();
    
    // Configuration methods
    void setLogLevel(LogLevel level) { logLevel_ = level; }
    void enableFileOutput(const std::string& filename);
    void disableFileOutput();
    void enableConsoleOutput(bool enable) { consoleOutput_ = enable; }
    void enableTimestamps(bool enable) { timestamps_ = enable; }
    
    // Core logging methods
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    // Vulkan-specific logging helpers
    void vulkanInfo(const std::string& operation, const std::string& details = "");
    void vulkanError(const std::string& operation, const std::string& error);
    void resourceInfo(const std::string& resourceType, const std::string& action, const std::string& name = "");
    
    // Performance logging
    void performanceLog(const std::string& operation, double milliseconds);
    
    // Flush all outputs
    void flush();

private:
    Logger() = default;
    ~Logger();
    
    // Disable copy/move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    
    // Helper methods
    std::string getLevelString(LogLevel level) const;
    std::string getCurrentTimestamp() const;
    void writeToOutputs(const std::string& formattedMessage);
    
    // Configuration
    LogLevel logLevel_ = LogLevel::INFO_LEVEL;
    bool consoleOutput_ = true;
    bool timestamps_ = true;
    
    // File output
    std::unique_ptr<std::ofstream> fileStream_;
    std::string currentLogFile_;
    
    // Thread safety
    std::mutex logMutex_;
};

} // namespace VulkanMon

// Convenient macros for logging (only active in debug builds for performance)
#ifdef DEBUG
    #define VKMON_DEBUG(msg)    VulkanMon::Logger::getInstance().debug(msg)
    #define VKMON_INFO(msg)     VulkanMon::Logger::getInstance().info(msg)
    #define VKMON_WARNING(msg)  VulkanMon::Logger::getInstance().warning(msg)
    #define VKMON_ERROR(msg)    VulkanMon::Logger::getInstance().error(msg)
    #define VKMON_FATAL(msg)    VulkanMon::Logger::getInstance().fatal(msg)
    
    #define VKMON_VK_INFO(op, details)    VulkanMon::Logger::getInstance().vulkanInfo(op, details)
    #define VKMON_VK_ERROR(op, error)     VulkanMon::Logger::getInstance().vulkanError(op, error)
    #define VKMON_RESOURCE(type, action, name) VulkanMon::Logger::getInstance().resourceInfo(type, action, name)
    #define VKMON_PERF(op, ms)            VulkanMon::Logger::getInstance().performanceLog(op, ms)
#else
    // In release builds, logging is minimal for performance
    #define VKMON_DEBUG(msg)    ((void)0)
    #define VKMON_INFO(msg)     VulkanMon::Logger::getInstance().info(msg)
    #define VKMON_WARNING(msg)  VulkanMon::Logger::getInstance().warning(msg)  
    #define VKMON_ERROR(msg)    VulkanMon::Logger::getInstance().error(msg)
    #define VKMON_FATAL(msg)    VulkanMon::Logger::getInstance().fatal(msg)
    
    #define VKMON_VK_INFO(op, details)    ((void)0)
    #define VKMON_VK_ERROR(op, error)     VulkanMon::Logger::getInstance().vulkanError(op, error)
    #define VKMON_RESOURCE(type, action, name) ((void)0)
    #define VKMON_PERF(op, ms)            ((void)0)
#endif