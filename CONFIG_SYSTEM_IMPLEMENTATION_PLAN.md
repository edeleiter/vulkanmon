# VulkanMon Configuration System Implementation Plan

## Overview

Design and implement a comprehensive, industry-standard configuration system for VulkanMon that balances performance, flexibility, and developer experience. Following VulkanMon's core philosophies: "Simple is Powerful", "Test as we go", and "Document often".

## Current State Analysis

### Existing Infrastructure
- ✅ **Config::Camera** - Compile-time constants in `src/config/CameraConfig.h`
- ✅ **Logger System** - Thread-safe logging with enum LogLevel (DEBUG, INFO, WARNING, ERROR, FATAL)
- ✅ **vcpkg Package Manager** - Can easily add nlohmann/json dependency
- ✅ **Clean Architecture** - Well-structured src/ directory with clear separation

### Problem Statement
- Log levels are hardcoded and cannot be adjusted at runtime
- No centralized configuration management
- Development vs production settings require recompilation
- Missing flexibility for different deployment environments

## Architecture Design

### 1. Hybrid Configuration Strategy

**Compile-Time Configs** (Performance Critical)
- Camera projection settings
- Buffer sizes, memory pools
- Rendering constants that affect GPU performance

**Runtime Configs** (Development/Deployment Flexibility)
- Log levels and output destinations
- Debug flags and validation layers
- Window settings, input sensitivity
- Asset loading paths

**Priority Order** (Highest to Lowest)
1. Command-line arguments (`--log-level=DEBUG`)
2. Environment variables (`VKMON_LOG_LEVEL=DEBUG`)
3. JSON configuration files (`config/vulkanmon.json`)
4. Compile-time defaults (`Config::Logging::DEFAULT_LOG_LEVEL`)

### 2. Directory Structure

```
src/config/
├── CameraConfig.h          # Existing - compile-time camera settings
├── LoggingConfig.h         # NEW - logging configuration
├── DebugConfig.h           # NEW - debug flags and validation
├── ConfigManager.h/.cpp    # NEW - runtime config loading and management
└── ConfigTypes.h           # NEW - shared types and enums
```

```
config/                     # NEW - configuration files directory
├── vulkanmon.json          # Default configuration
├── development.json        # Development overrides
├── production.json         # Production settings
└── README.md              # Configuration documentation
```

## Implementation Plan

### Phase 1: Core Infrastructure (Foundation)

#### 1.1 Add JSON Dependency
- Add `nlohmann-json` to `vcpkg.json`
- Update CMake for JSON linking
- Test JSON parsing in isolated unit test

#### 1.2 Create Config Types and Enums
**File:** `src/config/ConfigTypes.h`
```cpp
namespace VulkanMon::Config {
    enum class LogLevel {
        DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3, FATAL = 4
    };

    enum class ConfigSource {
        DEFAULT, JSON_FILE, ENVIRONMENT, COMMAND_LINE
    };

    struct ConfigMetadata {
        ConfigSource source;
        std::string sourceDetail; // filename, env var name, etc.
        std::chrono::time_point<std::chrono::steady_clock> loadTime;
    };
}
```

#### 1.3 Create Logging Configuration
**File:** `src/config/LoggingConfig.h`
```cpp
namespace VulkanMon::Config {
    struct Logging {
        // Runtime configurable settings
        static LogLevel currentLogLevel;
        static bool enableConsoleOutput;
        static bool enableFileOutput;
        static std::string logFilePath;
        static bool enableTimestamps;
        static bool enableThreadIds;

        // Compile-time defaults (fallback values)
        static constexpr LogLevel DEFAULT_LOG_LEVEL = LogLevel::INFO;
        static constexpr bool DEFAULT_CONSOLE_OUTPUT = true;
        static constexpr bool DEFAULT_FILE_OUTPUT = false;
        static constexpr const char* DEFAULT_LOG_FILE = "vulkanmon.log";
    };
}
```

### Phase 2: Configuration Manager (Core Loading Logic)

#### 2.1 Implement ConfigManager Class
**File:** `src/config/ConfigManager.h/.cpp`

**Responsibilities:**
- Load JSON configuration files with error handling
- Parse environment variables with validation
- Parse command-line arguments
- Apply priority-based configuration resolution
- Provide thread-safe configuration access
- Hot-reload support for development

**Key Methods:**
```cpp
class ConfigManager {
public:
    static ConfigManager& getInstance(); // Singleton

    // Loading methods
    void loadFromFile(const std::string& filepath);
    void loadFromEnvironment();
    void loadFromCommandLine(int argc, char* argv[]);
    void loadDefaults();

    // Access methods
    template<typename T>
    T getConfig(const std::string& key) const;

    ConfigMetadata getConfigMetadata(const std::string& key) const;

    // Development features
    void enableHotReload(const std::string& configPath);
    void reloadConfiguration();

private:
    std::map<std::string, ConfigValue> configs_;
    std::map<std::string, ConfigMetadata> metadata_;
    mutable std::shared_mutex configMutex_;
};
```

#### 2.2 JSON Configuration Schema
**File:** `config/vulkanmon.json`
```json
{
  "$schema": "./schema/vulkanmon-config.schema.json",
  "logging": {
    "level": "INFO",
    "enableConsoleOutput": true,
    "enableFileOutput": false,
    "logFilePath": "logs/vulkanmon.log",
    "enableTimestamps": true,
    "enableThreadIds": false
  },
  "debug": {
    "enableGPUValidation": false,
    "enableImGuiDemo": false,
    "enablePerformanceOverlay": false,
    "enableSpatialDebugDraw": false
  },
  "rendering": {
    "enableVSync": true,
    "targetFPS": 60,
    "maxInstancesPerBatch": 1000
  },
  "input": {
    "mouseSensitivity": 0.01,
    "cameraSpeed": 2.5
  }
}
```

### Phase 3: Logger Integration

#### 3.1 Update Logger to Use ConfigManager
**Modifications to:** `src/utils/Logger.h/.cpp`

- Replace hardcoded log level with `Config::Logging::currentLogLevel`
- Add configuration reload capability
- Implement dynamic log level changing
- Add configuration source reporting in debug logs

#### 3.2 Environment Variable Support
**Supported Variables:**
```bash
VKMON_LOG_LEVEL=DEBUG        # Override log level
VKMON_LOG_FILE=custom.log    # Override log file path
VKMON_CONFIG_FILE=dev.json   # Override config file
VKMON_DEBUG_MODE=1           # Enable debug features
```

### Phase 4: Command-Line Interface

#### 4.1 Add Command-Line Parsing
**Dependencies:** Add `cxxopts` to vcpkg.json for clean argument parsing

**Supported Arguments:**
```bash
./vulkanmon.exe --help                          # Show help
./vulkanmon.exe --config=development.json       # Use specific config
./vulkanmon.exe --log-level=DEBUG              # Override log level
./vulkanmon.exe --log-file=debug.log           # Override log file
./vulkanmon.exe --debug                        # Enable debug mode
./vulkanmon.exe --no-validation                # Disable GPU validation
```

#### 4.2 Help System and Documentation
- Auto-generated help from configuration schema
- Configuration value validation and error reporting
- Clear error messages for invalid configurations

### Phase 5: Advanced Features

#### 5.1 Configuration Validation
- JSON schema validation for configuration files
- Range checking for numeric values
- Path validation for file/directory settings
- Dependency validation (e.g., file output requires valid path)

#### 5.2 Hot Reload Support (Development Feature)
- File watcher for configuration files in debug builds
- Automatic configuration reload on file changes
- Thread-safe configuration updates
- Visual feedback for configuration changes

#### 5.3 Configuration Profiles
- Support for environment-specific configuration files
- Profile inheritance (production.json extends vulkanmon.json)
- Profile switching via command-line or environment variable

### Phase 6: Testing and Documentation

#### 6.1 Comprehensive Unit Tests
```cpp
// Test coverage areas
- JSON parsing with malformed files
- Environment variable precedence
- Command-line argument parsing
- Configuration hot-reload
- Thread-safety of configuration access
- Default fallback behavior
- Validation error handling
```

#### 6.2 Integration Tests
- Full application startup with different configuration scenarios
- Logger behavior validation with different log levels
- Performance impact measurement
- Memory usage validation

#### 6.3 Documentation
- Configuration reference documentation
- Development setup guides
- Deployment configuration examples
- Troubleshooting guides

## Implementation Timeline

### Week 1: Foundation
- [ ] Add JSON dependency and basic parsing
- [ ] Create ConfigTypes.h and LoggingConfig.h
- [ ] Implement basic ConfigManager skeleton
- [ ] Write unit tests for JSON parsing

### Week 2: Core Functionality
- [ ] Complete ConfigManager implementation
- [ ] Add environment variable support
- [ ] Integrate with existing Logger
- [ ] Create default configuration files

### Week 3: Advanced Features
- [ ] Add command-line argument parsing
- [ ] Implement configuration validation
- [ ] Add hot-reload support
- [ ] Comprehensive testing

### Week 4: Polish and Integration
- [ ] Performance optimization
- [ ] Documentation completion
- [ ] Integration testing with full application
- [ ] Production readiness validation

## Success Metrics

### Functional Requirements
- ✅ Log level can be changed without recompilation
- ✅ Configuration loading works from JSON, environment, and command-line
- ✅ Priority system correctly resolves configuration conflicts
- ✅ Thread-safe configuration access
- ✅ Graceful error handling for invalid configurations

### Performance Requirements
- ✅ Configuration loading time < 10ms for typical config files
- ✅ Configuration access overhead < 1µs per call
- ✅ Memory overhead < 1MB for configuration system
- ✅ No impact on frame rate or rendering performance

### Developer Experience Requirements
- ✅ Simple JSON configuration format
- ✅ Clear error messages for configuration issues
- ✅ Hot-reload works reliably in development
- ✅ Documentation is comprehensive and up-to-date

## Risk Mitigation

### Configuration File Corruption
- **Risk:** Invalid JSON breaks application startup
- **Mitigation:** Robust parsing with fallback to defaults, clear error messages

### Performance Impact
- **Risk:** Configuration access slows down hot paths
- **Mitigation:** Lazy loading, caching, compile-time defaults for critical paths

### Thread Safety
- **Risk:** Race conditions during configuration updates
- **Mitigation:** Shared mutex, atomic updates, comprehensive threading tests

### Backward Compatibility
- **Risk:** Existing code breaks when config system is introduced
- **Mitigation:** Gradual migration, maintain existing interfaces, extensive testing

## Future Extensions

### Phase 7+: Advanced Configuration Features
- Configuration versioning and migration
- Encrypted configuration support for sensitive data
- Remote configuration loading for deployment management
- Configuration analytics and usage tracking
- GUI configuration editor integration with ImGui

---

**Next Steps:** Begin implementation with Phase 1, starting with adding JSON dependency and creating the basic configuration types. Each phase should be completed and tested before moving to the next phase.