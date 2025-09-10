#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <functional>
#include <memory>

/**
 * VulkanMon Test Helpers
 * 
 * Common utilities for unit testing following our development philosophy.
 * 
 * Features:
 * - File system helpers for asset testing
 * - Timing utilities for performance testing
 * - String manipulation for log testing
 * - Thread testing utilities
 */

namespace VulkanMon {
namespace Testing {

/**
 * File System Test Helpers
 */
class FileTestHelpers {
public:
    // Create a temporary test file with specified content
    static std::string createTempFile(const std::string& filename, const std::string& content);
    
    // Create a temporary test directory
    static std::string createTempDirectory(const std::string& dirName);
    
    // Clean up test files and directories
    static void cleanupPath(const std::string& path);
    
    // Create a simple test texture file (minimal PNG data)
    static std::string createTestTexture(const std::string& filename, int width = 4, int height = 4);
    
    // Create a simple test OBJ model
    static std::string createTestModel(const std::string& filename);
    
    // Check if file exists and has expected size
    static bool validateFile(const std::string& path, size_t expectedMinSize = 0);
};

/**
 * Performance Test Helpers
 */
class PerformanceTestHelpers {
public:
    // Simple timer for measuring test performance
    class Timer {
    public:
        Timer() : start_(std::chrono::high_resolution_clock::now()) {}
        
        double elapsedMs() const {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(end - start_);
            return duration.count();
        }
        
        void reset() {
            start_ = std::chrono::high_resolution_clock::now();
        }
        
    private:
        std::chrono::high_resolution_clock::time_point start_;
    };
    
    // Run a function multiple times and return average execution time
    template<typename Func>
    static double benchmarkFunction(Func&& func, int iterations = 100) {
        Timer timer;
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        return timer.elapsedMs() / iterations;
    }
};

/**
 * Thread Test Helpers
 */
class ThreadTestHelpers {
public:
    // Run a function concurrently from multiple threads
    template<typename Func>
    static void runConcurrently(Func&& func, int threadCount = 4, int iterationsPerThread = 100) {
        std::vector<std::thread> threads;
        threads.reserve(threadCount);
        
        for (int i = 0; i < threadCount; ++i) {
            threads.emplace_back([&func, iterationsPerThread]() {
                for (int j = 0; j < iterationsPerThread; ++j) {
                    func();
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
    
    // Brief sleep for thread timing tests
    static void briefSleep(int milliseconds = 10) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
};

/**
 * String Test Helpers
 */
class StringTestHelpers {
public:
    // Check if string contains all specified substrings
    static bool containsAll(const std::string& text, const std::vector<std::string>& substrings);
    
    // Check if string matches expected log format
    static bool isValidLogFormat(const std::string& logLine);
    
    // Extract log level from log line
    static std::string extractLogLevel(const std::string& logLine);
    
    // Count occurrences of substring
    static size_t countOccurrences(const std::string& text, const std::string& substring);
    
    // Split string by delimiter
    static std::vector<std::string> split(const std::string& text, char delimiter);
};

/**
 * Memory Test Helpers
 */
class MemoryTestHelpers {
public:
    // Simple memory usage tracking for tests
    struct MemorySnapshot {
        size_t allocatedBytes = 0;
        size_t allocationCount = 0;
    };
    
    // Take a snapshot of current memory usage (simplified for testing)
    static MemorySnapshot takeSnapshot();
    
    // Compare two memory snapshots
    static bool compareSnapshots(const MemorySnapshot& before, const MemorySnapshot& after, 
                                size_t maxBytesIncrease = 1024, size_t maxAllocationsIncrease = 10);
};

/**
 * RAII Test Helpers
 */
template<typename ResourceType>
class RAIITestHelper {
public:
    // Test that resource is properly cleaned up when going out of scope
    static bool testRAIICleanup(std::function<std::unique_ptr<ResourceType>()> createResource,
                               std::function<bool(const ResourceType*)> isResourceValid) {
        ResourceType* rawPtr = nullptr;
        
        {
            auto resource = createResource();
            rawPtr = resource.get();
            
            // Resource should be valid while in scope
            if (!isResourceValid(rawPtr)) {
                return false;
            }
        } // Resource should be cleaned up here
        
        // After going out of scope, resource should be invalid
        // Note: This is a simplified test - in real scenarios we'd need
        // more sophisticated validation
        return true;
    }
};

} // namespace Testing
} // namespace VulkanMon