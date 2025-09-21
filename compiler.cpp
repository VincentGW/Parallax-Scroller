#include <iostream>
#include <string>
#include <cstdlib>
#include <filesystem>

int main(int argc, char* argv[]) {
    std::cout << "SFML Parallax Scroller Compiler" << std::endl;
    std::cout << "===============================" << std::endl;

    // Check if main.cpp exists
    if (!std::filesystem::exists("main.cpp")) {
        std::cerr << "Error: main.cpp not found in current directory!" << std::endl;
        return 1;
    }

    // Build the g++ command with full path to MSYS2 g++
    std::string gppPath = "C:/msys64/mingw64/bin/g++.exe";
    std::string command = "\"" + gppPath + "\" -std=c++17 main.cpp -o ParallaxScroller.exe -lsfml-graphics -lsfml-window -lsfml-system";

    std::cout << "Compiling SFML Parallax Scroller..." << std::endl;
    std::cout << "Command: " << command << std::endl;

    // Execute the compilation
    int result = system(command.c_str());

    if (result == 0) {
        std::cout << std::endl;
        std::cout << "Build successful!" << std::endl;
        std::cout << "Created: ParallaxScroller.exe" << std::endl;
        std::cout << std::endl;
        std::cout << "To run: ./ParallaxScroller.exe or double-click ParallaxScroller.exe" << std::endl;

        // Check if executable was actually created
        if (std::filesystem::exists("ParallaxScroller.exe")) {
            std::cout << "Executable verified!" << std::endl;
        } else {
            std::cout << "Warning: Executable not found after build" << std::endl;
        }
    } else {
        std::cout << std::endl;
        std::cerr << "Build failed!" << std::endl;
        std::cerr << "Make sure SFML is installed in MSYS2:" << std::endl;
        std::cerr << "  1. Open MSYS2 terminal" << std::endl;
        std::cerr << "  2. Run: pacman -S mingw-w64-x86_64-sfml" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Or check that the g++ path is correct:" << std::endl;
        std::cerr << "  " << gppPath << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Press Enter to exit...";
    std::cin.get();

    return result;
}