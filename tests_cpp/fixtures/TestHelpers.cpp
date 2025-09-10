#include "TestHelpers.h"
#include <fstream>

namespace VulkanMon {
namespace Testing {

// ============================================================================
// FileTestHelpers Implementation (Minimal for Phase 1)
// ============================================================================

std::string FileTestHelpers::createTempFile(const std::string& filename, const std::string& content) {
    std::string tempPath = "temp_" + filename;
    std::ofstream file(tempPath);
    if (file.is_open()) {
        file << content;
        file.close();
    }
    return tempPath;
}

std::string FileTestHelpers::createTempDirectory(const std::string& dirName) {
    std::string tempDir = "temp_" + dirName;
    std::filesystem::create_directory(tempDir);
    return tempDir;
}

void FileTestHelpers::cleanupPath(const std::string& path) {
    std::filesystem::remove_all(path);
}

std::string FileTestHelpers::createTestTexture(const std::string& filename, int width, int height) {
    // Create a minimal PNG-like file for testing
    // In Phase 2, this will create actual PNG data
    std::string content = "PNG_TEST_DATA_" + std::to_string(width) + "x" + std::to_string(height);
    return createTempFile(filename, content);
}

std::string FileTestHelpers::createTestModel(const std::string& filename) {
    // Create a minimal OBJ file for testing
    std::string objContent = 
        "# Test OBJ file\n"
        "v -1.0 -1.0 0.0\n"
        "v 1.0 -1.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 3\n";
    
    return createTempFile(filename, objContent);
}

bool FileTestHelpers::validateFile(const std::string& path, size_t expectedMinSize) {
    if (!std::filesystem::exists(path)) {
        return false;
    }
    
    auto fileSize = std::filesystem::file_size(path);
    return fileSize >= expectedMinSize;
}

// ============================================================================
// StringTestHelpers Implementation (Minimal for Phase 1)
// ============================================================================

bool StringTestHelpers::containsAll(const std::string& text, const std::vector<std::string>& substrings) {
    for (const auto& substring : substrings) {
        if (text.find(substring) == std::string::npos) {
            return false;
        }
    }
    return true;
}

bool StringTestHelpers::isValidLogFormat(const std::string& logLine) {
    // Simple validation - look for timestamp and log level
    return logLine.find("[") != std::string::npos && logLine.find("]") != std::string::npos;
}

std::string StringTestHelpers::extractLogLevel(const std::string& logLine) {
    size_t start = logLine.find("[");
    size_t end = logLine.find("]", start);
    if (start != std::string::npos && end != std::string::npos) {
        return logLine.substr(start + 1, end - start - 1);
    }
    return "";
}

size_t StringTestHelpers::countOccurrences(const std::string& text, const std::string& substring) {
    size_t count = 0;
    size_t pos = 0;
    while ((pos = text.find(substring, pos)) != std::string::npos) {
        count++;
        pos += substring.length();
    }
    return count;
}

std::vector<std::string> StringTestHelpers::split(const std::string& text, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

// ============================================================================
// MemoryTestHelpers Implementation (Minimal for Phase 1)
// ============================================================================

MemoryTestHelpers::MemorySnapshot MemoryTestHelpers::takeSnapshot() {
    // Minimal implementation for Phase 1
    // In Phase 2, this could integrate with actual memory tracking
    MemorySnapshot snapshot;
    snapshot.allocatedBytes = 0;
    snapshot.allocationCount = 0;
    return snapshot;
}

bool MemoryTestHelpers::compareSnapshots(const MemorySnapshot& before, const MemorySnapshot& after,
                                        size_t maxBytesIncrease, size_t maxAllocationsIncrease) {
    // Minimal implementation for Phase 1
    return (after.allocatedBytes - before.allocatedBytes) <= maxBytesIncrease &&
           (after.allocationCount - before.allocationCount) <= maxAllocationsIncrease;
}

} // namespace Testing
} // namespace VulkanMon