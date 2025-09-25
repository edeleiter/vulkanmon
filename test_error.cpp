#include "src/utils/Utils.h"
#include <iostream>
int main() {
    try {
        Utils::readFile("nonexistent_file.txt");
    } catch (const std::exception& e) {
        std::cout << "Error message: [" << e.what() << "]" << std::endl;
    }
    return 0;
}
