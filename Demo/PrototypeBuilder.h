#pragma once

#include <filesystem>
#include <string>

namespace PrototypeBuilder
{
    bool Build(const std::filesystem::path& projectFile, std::string* errorOut = nullptr);
}
