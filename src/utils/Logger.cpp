#include "Logger.h"
#include <iomanip>
#include <sstream>

namespace VulkanMon {

// ============================================================================
// Logger Implementation
// ============================================================================

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    flush();
    if (fileStream_) {
        fileStream_->close();
    }
}

void Logger::enableFileOutput(const std::string& filename) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    // Close existing file if open
    if (fileStream_) {
        fileStream_->close();
    }
    
    // Open new file
    fileStream_ = std::make_unique<std::ofstream>(filename, std::ios::app);
    if (fileStream_->is_open()) {
        currentLogFile_ = filename;
        // Log opening message directly to avoid recursive mutex lock
        std::cout << "[" << getCurrentTimestamp() << "] [INFO] Log file opened: " << filename << std::endl;
    } else {
        fileStream_.reset();
        std::cerr << "[LOGGER ERROR] Failed to open log file: " << filename << std::endl;
    }
}

void Logger::disableFileOutput() {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (fileStream_) {
        // Log closure message directly to avoid recursive mutex lock
        std::cout << "[" << getCurrentTimestamp() << "] [INFO] Closing log file: " << currentLogFile_ << std::endl;
        
        fileStream_->close();
        fileStream_.reset();
        currentLogFile_.clear();
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    // Early return if log level is too low
    if (level < logLevel_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(logMutex_);
    
    // Format the message
    std::ostringstream oss;
    
    if (timestamps_) {
        oss << "[" << getCurrentTimestamp() << "] ";
    }
    
    oss << "[" << getLevelString(level) << "] " << message;
    
    writeToOutputs(oss.str());
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG_LEVEL, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO_LEVEL, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING_LEVEL, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR_LEVEL, message);
}

void Logger::fatal(const std::string& message) {
    log(LogLevel::FATAL_LEVEL, message);
    flush(); // Ensure fatal messages are immediately written
}

void Logger::vulkanInfo(const std::string& operation, const std::string& details) {
    std::string msg = "[VULKAN] " + operation;
    if (!details.empty()) {
        msg += " - " + details;
    }
    info(msg);
}

void Logger::vulkanError(const std::string& operation, const std::string& errorMsg) {
    error("[VULKAN ERROR] " + operation + " - " + errorMsg);
}

void Logger::resourceInfo(const std::string& resourceType, const std::string& action, const std::string& name) {
    std::string msg = "[RESOURCE] " + resourceType + " " + action;
    if (!name.empty()) {
        msg += " (" + name + ")";
    }
    debug(msg);
}

void Logger::performanceLog(const std::string& operation, double milliseconds) {
    std::ostringstream oss;
    oss << "[PERFORMANCE] " << operation << " took " << std::fixed << std::setprecision(2) << milliseconds << "ms";
    debug(oss.str());
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (consoleOutput_) {
        std::cout.flush();
        std::cerr.flush();
    }
    
    if (fileStream_ && fileStream_->is_open()) {
        fileStream_->flush();
    }
}

std::string Logger::getLevelString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG_LEVEL:   return "DEBUG";
        case LogLevel::INFO_LEVEL:    return "INFO ";
        case LogLevel::WARNING_LEVEL: return "WARN ";
        case LogLevel::ERROR_LEVEL:   return "ERROR";
        case LogLevel::FATAL_LEVEL:   return "FATAL";
        default:                return "UNKNW";
    }
}

std::string Logger::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::writeToOutputs(const std::string& formattedMessage) {
    // Write to console
    if (consoleOutput_) {
        std::cout << formattedMessage << std::endl;
    }
    
    // Write to file
    if (fileStream_ && fileStream_->is_open()) {
        *fileStream_ << formattedMessage << std::endl;
    }
}

} // namespace VulkanMon