#include "pch.h"
#include "ConfigReader.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::unordered_map<std::string, std::string> loadConfig(const std::string& filename) {
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "[ConfigReader] No config file found: " << filename << "\n";
        return config; // return empty map
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key, eq, value;
        if (iss >> key >> eq >> value && eq == "=")
            config[key] = value;
    }

    return config;
}
