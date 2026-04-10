#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "Editor/Managers/ProjectManager.h"

class BuildPipeline
{
public:
    static void WriteRuntimeManifest(const Project& project, const std::filesystem::path& outputRoot)
    {
        nlohmann::json manifest = {
            { "runtimeId", project.runtimeIdentifier },
            { "startupScene", project.startupScenePath.lexically_relative(project.rootPath).generic_string() },
            { "assets", CollectRelativeFiles(outputRoot / "Assets", outputRoot) },
            { "scenes", CollectRelativeFiles(outputRoot / "Scenes", outputRoot) },
            { "config", CollectRelativeFiles(outputRoot / "Config", outputRoot) }
        };

        std::ofstream file(outputRoot / "Config" / "runtime_manifest.json");
        if (file.is_open())
        {
            file << manifest.dump(4);
        }
    }

private:
    static std::vector<std::string> CollectRelativeFiles(const std::filesystem::path& root, const std::filesystem::path& packageRoot)
    {
        std::vector<std::string> files;
        std::error_code ec;
        if (!std::filesystem::exists(root, ec))
        {
            return files;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec))
        {
            if (ec || !entry.is_regular_file())
            {
                continue;
            }

            const auto relativePath = std::filesystem::relative(entry.path(), packageRoot, ec);
            files.push_back(ec ? entry.path().generic_string() : relativePath.generic_string());
        }

        return files;
    }
};
