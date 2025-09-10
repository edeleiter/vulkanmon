#include "Utils.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

std::vector<char> Utils::readFile(const std::string& filename) {
    // Try multiple common locations for shader files
    std::vector<std::string> searchPaths = {
        filename,                    // Direct path (current working directory)
        "../" + filename,           // One level up (if running from build/)
        "../../" + filename,        // Two levels up
        "./" + filename             // Explicit current directory
    };
    
    std::ifstream file;
    std::string foundPath;
    
    for (const auto& path : searchPaths) {
        file.open(path, std::ios::ate | std::ios::binary);
        if (file.is_open()) {
            foundPath = path;
            break;
        }
    }
    
    if (!file.is_open()) {
        std::string errorMsg = "Failed to open file: " + filename + "\nSearched paths:\n";
        for (const auto& path : searchPaths) {
            errorMsg += "  - " + path + "\n";
        }
        throw std::runtime_error(errorMsg);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

bool Utils::recompileShaders() {
    std::cout << "[SHADER] Recompiling vertex shader..." << std::endl;
    // Go up one directory from build/ to access shaders/
    int vertResult = std::system("cd ../shaders && glslc triangle.vert -o vert.spv 2>&1");
    if (vertResult != 0) {
        std::cout << "[ERROR] Vertex shader compilation failed (exit code: " << vertResult << ")" << std::endl;
        std::cout << "[HINT] Check shaders/triangle.vert for syntax errors" << std::endl;
        return false;
    }
    std::cout << "[SHADER] Vertex shader compiled successfully" << std::endl;
    
    std::cout << "[SHADER] Recompiling fragment shader..." << std::endl;
    int fragResult = std::system("cd ../shaders && glslc triangle.frag -o frag.spv 2>&1");
    if (fragResult != 0) {
        std::cout << "[ERROR] Fragment shader compilation failed (exit code: " << fragResult << ")" << std::endl;
        std::cout << "[HINT] Check shaders/triangle.frag for syntax errors" << std::endl;
        return false;
    }
    std::cout << "[SHADER] Fragment shader compiled successfully" << std::endl;
    
    std::cout << "[SUCCESS] All shaders recompiled and ready!" << std::endl;
    return true;
}