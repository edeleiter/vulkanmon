#include "AssetManager.h"
#include "../utils/Logger.h"
#include "../rendering/ResourceManager.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <stb_image.h>

namespace VulkanMon {

// ============================================================================
// AssetManager Implementation
// ============================================================================

AssetManager::AssetManager(VkDevice device, VkPhysicalDevice physicalDevice, 
                           VkCommandPool commandPool, VkQueue graphicsQueue,
                           const std::string& assetsRootPath)
    : device_(device)
    , physicalDevice_(physicalDevice)
    , commandPool_(commandPool)
    , graphicsQueue_(graphicsQueue)
    , assetsRootPath_(assetsRootPath)
    , resourceManager_(std::make_unique<ResourceManager>(device, physicalDevice)) {
    
    initializeAssetDirectories();
    VKMON_INFO("AssetManager initialized with root path: " + assetsRootPath_);
}

AssetManager::~AssetManager() {
    VKMON_INFO("AssetManager cleanup starting");
    
    // Clear texture cache (this will destroy samplers)
    for (auto& [filename, texture] : textureCache_) {
        if (texture->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device_, texture->sampler, nullptr);
        }
    }
    textureCache_.clear();
    
    VKMON_INFO("AssetManager cleanup complete");
}

void AssetManager::initializeAssetDirectories() {
    assetSubdirectories_[AssetType::TEXTURE] = "textures/";
    assetSubdirectories_[AssetType::MODEL] = "models/";
    assetSubdirectories_[AssetType::SHADER] = "shaders/";
    assetSubdirectories_[AssetType::SCENE] = "scenes/";
    assetSubdirectories_[AssetType::AUDIO] = "audio/";
    
    // Verify directories exist
    for (const auto& [type, subdir] : assetSubdirectories_) {
        std::string fullPath = assetsRootPath_ + subdir;
        if (!std::filesystem::exists(fullPath)) {
            VKMON_WARNING("Asset directory does not exist: " + fullPath);
        } else {
            VKMON_DEBUG("Asset directory verified: " + fullPath);
        }
    }
}

std::string AssetManager::getAssetTypeDirectory(AssetType type) const {
    auto it = assetSubdirectories_.find(type);
    return (it != assetSubdirectories_.end()) ? it->second : "";
}

std::string AssetManager::getFullAssetPath(AssetType type, const std::string& filename) const {
    return assetsRootPath_ + getAssetTypeDirectory(type) + filename;
}

bool AssetManager::validateAssetPath(const std::string& relativePath) const {
    std::string fullPath = assetsRootPath_ + relativePath;
    return fileExists(fullPath);
}

bool AssetManager::fileExists(const std::string& path) const {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

std::string AssetManager::getFileExtension(const std::string& filename) const {
    size_t lastDot = filename.find_last_of('.');
    return (lastDot != std::string::npos) ? filename.substr(lastDot) : "";
}

// ============================================================================
// Texture Loading Implementation
// ============================================================================

std::shared_ptr<LoadedTexture> AssetManager::loadTexture(const std::string& filename) {
    TextureLoadInfo loadInfo(filename);
    return loadTexture(loadInfo);
}

std::shared_ptr<LoadedTexture> AssetManager::loadTexture(const TextureLoadInfo& loadInfo) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Check cache first
    auto it = textureCache_.find(loadInfo.filename);
    if (it != textureCache_.end()) {
        it->second->referenceCount++;
        VKMON_DEBUG("Texture loaded from cache: " + loadInfo.filename + " (refs: " + 
                   std::to_string(it->second->referenceCount) + ")");
        return it->second;
    }
    
    try {
        // Load image data
        uint32_t width, height;
        auto image = createTextureImage(loadInfo.filename, width, height);
        
        // Create sampler
        VkSampler sampler = createTextureSampler(loadInfo);
        
        // Create loaded texture
        auto loadedTexture = std::make_shared<LoadedTexture>(
            std::move(image), sampler, width, height, loadInfo.filename);
        loadedTexture->referenceCount = 1;
        
        // Add to cache
        textureCache_[loadInfo.filename] = loadedTexture;
        
        // Performance logging
        if (performanceLogging_) {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double, std::milli>(endTime - startTime);
            VKMON_PERF("Texture loading (" + loadInfo.debugName + ")", duration.count());
        }
        
        VKMON_INFO("Texture loaded: " + loadInfo.filename + " (" + 
                  std::to_string(width) + "x" + std::to_string(height) + ")");
        
        return loadedTexture;
        
    } catch (const std::exception& e) {
        VKMON_ERROR("Failed to load texture " + loadInfo.filename + ": " + e.what());
        throw;
    }
}

std::unique_ptr<ManagedImage> AssetManager::createTextureImage(const std::string& filename, 
                                                               uint32_t& width, uint32_t& height) {
    // Get full path
    std::string fullPath = getFullAssetPath(AssetType::TEXTURE, filename);
    
    if (!fileExists(fullPath)) {
        throw std::runtime_error("Texture file not found: " + fullPath);
    }
    
    // Load image with stb_image
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(fullPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    
    if (!pixels) {
        throw std::runtime_error("Failed to load texture image: " + fullPath);
    }
    
    width = static_cast<uint32_t>(texWidth);
    height = static_cast<uint32_t>(texHeight);
    VkDeviceSize imageSize = width * height * 4; // RGBA
    
    // Create staging buffer
    auto stagingBuffer = resourceManager_->createBuffer(
        imageSize, 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "texture_staging_" + filename
    );
    
    // Copy pixel data to staging buffer
    stagingBuffer->updateData(pixels, imageSize);
    
    // Free the loaded image
    stbi_image_free(pixels);
    
    // Create texture image
    auto textureImage = resourceManager_->createImage(
        width, height,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        "texture_" + filename
    );
    
    // Transition image layout and copy data
    transitionImageLayout(textureImage->getImage(), VK_FORMAT_R8G8B8A8_SRGB, 
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    copyBufferToImage(stagingBuffer->getBuffer(), textureImage->getImage(), width, height);
    
    transitionImageLayout(textureImage->getImage(), VK_FORMAT_R8G8B8A8_SRGB,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    return textureImage;
}

VkSampler AssetManager::createTextureSampler(const TextureLoadInfo& loadInfo) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = loadInfo.magFilter;
    samplerInfo.minFilter = loadInfo.minFilter;
    samplerInfo.addressModeU = loadInfo.addressMode;
    samplerInfo.addressModeV = loadInfo.addressMode;
    samplerInfo.addressModeW = loadInfo.addressMode;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    VkSampler sampler;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture sampler for: " + loadInfo.filename);
    }
    
    return sampler;
}

void AssetManager::unloadTexture(const std::string& filename) {
    auto it = textureCache_.find(filename);
    if (it != textureCache_.end()) {
        it->second->referenceCount--;
        VKMON_DEBUG("Texture reference decremented: " + filename + " (refs: " + 
                   std::to_string(it->second->referenceCount) + ")");
        
        if (it->second->referenceCount == 0) {
            VKMON_INFO("Texture unloaded: " + filename);
            if (it->second->sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device_, it->second->sampler, nullptr);
            }
            textureCache_.erase(it);
        }
    }
}

// ============================================================================
// Command Buffer Helpers
// ============================================================================

VkCommandBuffer AssetManager::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

void AssetManager::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void AssetManager::transitionImageLayout(VkImage image, VkFormat format, 
                                         VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

void AssetManager::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

// ============================================================================
// Asset Discovery and Management
// ============================================================================

std::vector<std::string> AssetManager::discoverAssets(AssetType type, const std::string& extension) {
    std::vector<std::string> assets;
    std::string typeDir = getAssetTypeDirectory(type);
    
    if (typeDir.empty()) {
        VKMON_WARNING("Unknown asset type for discovery");
        return assets;
    }
    
    std::string searchPath = assetsRootPath_ + typeDir;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(searchPath)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                if (extension.empty() || getFileExtension(filename) == extension) {
                    assets.push_back(filename);
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        VKMON_ERROR("Failed to discover assets in " + searchPath + ": " + e.what());
    }
    
    return assets;
}

void AssetManager::clearTextureCache() {
    VKMON_INFO("Clearing texture cache (" + std::to_string(textureCache_.size()) + " textures)");
    
    for (auto& [filename, texture] : textureCache_) {
        if (texture->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device_, texture->sampler, nullptr);
        }
    }
    
    textureCache_.clear();
}

size_t AssetManager::getTotalTextureMemory() const {
    size_t total = 0;
    for (const auto& [filename, texture] : textureCache_) {
        total += texture->width * texture->height * 4; // RGBA
    }
    return total;
}

void AssetManager::printAssetSummary() const {
    VKMON_INFO("\n=== ASSET MANAGER SUMMARY ===");
    VKMON_INFO("Loaded Textures: " + std::to_string(textureCache_.size()));
    VKMON_INFO("Texture Memory: " + std::to_string(getTotalTextureMemory()) + " bytes");
    
    if (resourceManager_) {
        resourceManager_->printResourceSummary();
    }
    
    VKMON_INFO("=============================");
}

} // namespace VulkanMon