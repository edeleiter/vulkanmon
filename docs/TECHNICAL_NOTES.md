# Technical Notes and Fixes

## Logger Recursive Mutex Deadlock Fix

### Problem
The Logger class had a recursive mutex deadlock in `enableFileOutput()` and `disableFileOutput()` methods:

```cpp
void Logger::disableFileOutput() {
    std::lock_guard<std::mutex> lock(logMutex_);  // Acquire mutex
    if (fileStream_) {
        log(LogLevel::INFO_LEVEL, "Closing log file: " + currentLogFile_);  // Calls log() which tries to acquire same mutex
        // DEADLOCK!
    }
}
```

### Root Cause
Both methods held `logMutex_` while calling `log()`, which attempted to acquire the same mutex again, causing a resource deadlock.

### Solution
Replaced recursive `log()` calls with direct console output:

```cpp
void Logger::disableFileOutput() {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (fileStream_) {
        // Direct output avoids recursive mutex lock
        std::cout << "[" << getCurrentTimestamp() << "] [INFO] Closing log file: " << currentLogFile_ << std::endl;
        fileStream_->close();
        fileStream_.reset();
        currentLogFile_.clear();
    }
}
```

### Files Modified
- `src/Logger.cpp`: Fixed `enableFileOutput()` and `disableFileOutput()` methods
- Lines 35 and 46: Replaced `log()` calls with direct `std::cout` output

### Test Results
- **Before**: Logger file test failed with "resource deadlock would occur"
- **After**: All 10 test cases pass (59 assertions total)

## vcpkg Integration Updates

### Test System CMakeLists.txt
Updated `tests_cpp/CMakeLists.txt` for proper vcpkg compatibility:

```cmake
# Before
find_package(glfw3 REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# After  
find_package(glfw3 CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)
```

### Key Changes
1. Added `CONFIG` mode for vcpkg packages
2. Removed problematic CTest discovery (optional feature)
3. Simplified Catch2 integration for cross-platform compatibility

### Result
All C++ tests now use vcpkg-managed dependencies consistently across Windows and Linux.