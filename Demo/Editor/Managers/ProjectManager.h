#pragma once
#include <string>
#include <filesystem>


struct Project
{
    std::string name;
    std::string runtimeIdentifier;
    std::filesystem::path rootPath;
    std::filesystem::path projectFile;
    std::filesystem::path assetPath;
    std::filesystem::path scenePath;
    std::filesystem::path startupScenePath;
    std::filesystem::path configPath;
    std::filesystem::path buildOutputPath;
};

class ProjectManager
{
public:
    static void Create(const std::filesystem::path& rootPath);
    static bool Load(const std::filesystem::path& rootPath);
    static void LoadRuntimeLayout(
        const std::string& projectName,
        const std::filesystem::path& runtimeRoot,
        const std::filesystem::path& assetDir,
        const std::filesystem::path& startupScene,
        const std::filesystem::path& configPath);

    static bool HasActiveProject();
    static const Project& GetActive();
    static void SetStartupScene(const std::filesystem::path& scenePath);
    static void UpdateBuildSettings(
        const std::filesystem::path& startupScene,
        const std::filesystem::path& buildOutputPath,
        const std::string& runtimeIdentifier);

private:
    static Project s_ActiveProject;
    static bool s_HasProject;
};
