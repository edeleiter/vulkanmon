#pragma once

#include "../rendering/ResourceManager.h"
#include "../utils/Logger.h"
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include <shared_mutex>
#include <atomic>

/**
 * VulkanMon Asset Loading Pipeline
 * 
 * Manages loading and caching of game assets following our philosophy:
 * - "Simple is Powerful" - Clear asset types, straightforward loading API
 * - "Test, Test, Test" - Robust error handling and validation
 * - "Document Often" - Clear asset organization and usage patterns
 * 
 * Features:
 * - Texture loading from assets/textures/
 * - Model loading preparation (for Assimp integration)
 * - Shader loading and compilation
 * - Asset caching and reference counting
 * - Automatic resource cleanup
 * - Performance monitoring
 */

namespace VulkanMon {

// Forward declarations
class ResourceManager;
class ManagedImage;
class ManagedBuffer;

/**
 * Asset types supported by the pipeline
 */
enum class AssetType {
    TEXTURE,
    MODEL,
    SHADER,
    SCENE,
    AUDIO
};

/**
 * Texture loading configuration
 */
struct TextureLoadInfo {
    std::string filename;
    bool generateMipmaps = false;
    VkFilter magFilter = VK_FILTER_LINEAR;
    VkFilter minFilter = VK_FILTER_LINEAR;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    std::string debugName;
    
    TextureLoadInfo(const std::string& file) : filename(file) {
        // Generate debug name from filename
        size_t lastSlash = file.find_last_of("/\\");
        debugName = (lastSlash != std::string::npos) ? file.substr(lastSlash + 1) : file;
    }
};

/**
 * Loaded texture resource
 */
struct LoadedTexture {
    std::unique_ptr<ManagedImage> image;
    VkSampler sampler = VK_NULL_HANDLE;
    uint32_t width, height;
    std::string filename;
    std::atomic<size_t> referenceCount = 0;
    
    LoadedTexture(std::unique_ptr<ManagedImage> img, VkSampler samp, 
                  uint32_t w, uint32_t h, const std::string& file)
        : image(std::move(img)), sampler(samp), width(w), height(h), filename(file) {}
        
    ~LoadedTexture() {
        if (sampler != VK_NULL_HANDLE) {
            // Note: Device cleanup handled by AssetManager
        }
    }
};

/**
 * Asset Manager - Central hub for all asset loading and management
 */
class AssetManager {
public:
    explicit AssetManager(VkDevice device, VkPhysicalDevice physicalDevice, 
                          VkCommandPool commandPool, VkQueue graphicsQueue,
                          const std::string& assetsRootPath = "assets/");
    ~AssetManager();
    
    // Move-only semantics
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    AssetManager(AssetManager&&) = default;
    AssetManager& operator=(AssetManager&&) = default;
    
    // Texture loading
    std::shared_ptr<LoadedTexture> loadTexture(const TextureLoadInfo& loadInfo);
    std::shared_ptr<LoadedTexture> loadTexture(const std::string& filename);
    void unloadTexture(const std::string& filename);
    
    // Shader loading and compilation
    std::vector<char> loadShaderBytecode(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& bytecode, const std::string& debugName = "");
    bool compileShader(const std::string& sourcePath, const std::string& outputPath, const std::string& shaderType);
    
    // Asset discovery and validation
    std::vector<std::string> discoverAssets(AssetType type, const std::string& extension = "");
    bool validateAssetPath(const std::string& relativePath) const;
    std::string getFullAssetPath(AssetType type, const std::string& filename) const;
    
    // Cache management
    void clearTextureCache();
    size_t getTextureCount() const {
        std::shared_lock<std::shared_mutex> lock(textureCacheMutex_);
        return textureCache_.size();
    }
    size_t getTotalTextureMemory() const;
    
    // Performance and debugging
    void printAssetSummary() const;
    void enablePerformanceLogging(bool enable) { performanceLogging_ = enable; }

private:
    // Core Vulkan objects
    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    VkCommandPool commandPool_;
    VkQueue graphicsQueue_;
    
    // Asset organization
    std::string assetsRootPath_;
    std::unordered_map<AssetType, std::string> assetSubdirectories_;
    
    // Resource management
    std::unique_ptr<ResourceManager> resourceManager_;
    
    // Asset caches
    std::unordered_map<std::string, std::shared_ptr<LoadedTexture>> textureCache_;
    mutable std::shared_mutex textureCacheMutex_;  // Protects textureCache_
    
    // Configuration
    bool performanceLogging_ = true;
    
    // Helper methods
    void initializeAssetDirectories();
    std::string getAssetTypeDirectory(AssetType type) const;
    
    // Texture loading helpers
    std::unique_ptr<ManagedImage> createTextureImage(const std::string& filename, uint32_t& width, uint32_t& height);
    VkSampler createTextureSampler(const TextureLoadInfo& loadInfo);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    
    // File system helpers
    bool fileExists(const std::string& path) const;
    std::string getFileExtension(const std::string& filename) const;
};

} // namespace VulkanMon