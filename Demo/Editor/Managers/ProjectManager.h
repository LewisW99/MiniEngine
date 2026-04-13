#pragma once
#include <string>
#include <filesystem>
#include <vector>

struct ProjectSceneInfo
{
    std::string id;
    std::string name;
    std::filesystem::path path;
};

struct BuildSceneInfo
{
    std::string sceneId;
    int buildIndex = 0;
    bool included = false;
};

struct ProjectCreateOptions
{
    std::string initialSceneName{ "Main" };
};

struct Project
{
    std::string name;
    std::string runtimeIdentifier;
    std::filesystem::path rootPath;
    std::filesystem::path projectFile;
    std::filesystem::path assetPath;
    std::filesystem::path sceneDirectory;
    std::filesystem::path scenePath;
    std::filesystem::path startupScenePath;
    std::filesystem::path configPath;
    std::filesystem::path buildOutputPath;
    std::string startupSceneId;
    std::vector<ProjectSceneInfo> scenes;
    std::vector<BuildSceneInfo> buildScenes;
};

class ProjectManager
{
public:
    static void Create(const std::filesystem::path& rootPath, const ProjectCreateOptions& options = {});
    static bool Load(const std::filesystem::path& rootPath);
    static void LoadRuntimeLayout(
        const std::string& projectName,
        const std::filesystem::path& runtimeRoot,
        const std::filesystem::path& assetDir,
        const std::filesystem::path& startupScene,
        const std::filesystem::path& configPath);

    static bool HasActiveProject();
    static const Project& GetActive();
    static const std::vector<ProjectSceneInfo>& GetScenes();
    static const std::vector<BuildSceneInfo>& GetBuildScenes();
    static bool CreateScene(const std::string& sceneName, std::filesystem::path* createdScenePath = nullptr);
    static bool OpenScene(const std::filesystem::path& scenePath);
    static void SetStartupScene(const std::filesystem::path& scenePath);
    static void UpdateBuildSettings(const std::filesystem::path& buildOutputPath,
        const std::string& runtimeIdentifier,
        const std::string& startupSceneId,
        const std::vector<BuildSceneInfo>& buildScenes);

private:
    static Project s_ActiveProject;
    static bool s_HasProject;
};
