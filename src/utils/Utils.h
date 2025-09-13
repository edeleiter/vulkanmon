#pragma once

#include <vector>
#include <string>

class Utils {
public:
    static std::vector<char> readFile(const std::string& filename);
    static bool recompileShaders();
};