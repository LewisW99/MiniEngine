#include "ProjectManager.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../../../Engine/SettingsManager.h"
#include "../../../Engine/SceneSerializer.h"
#include "../../../Engine/ECS/ComponentManager.h"
#include "../../../Engine/ECS/EntityManager.h"
#include "../../../Engine/ECS/EntityMeta.h"

using json = nlohmann::json;

Project ProjectManager::s_ActiveProject{};
bool ProjectManager::s_HasProject = false;

namespace
{
    std::string ToStoredPath(const std::filesystem::path& path, const std::filesystem::path& rootPath)
    {
        if (path.empty())
        {
            return {};
        }

        std::error_code ec;
        const auto relativePath = std::filesystem::relative(path, rootPath, ec);
        return ec ? path.generic_string() : relativePath.generic_string();
    }

    json BuildProjectJson(const Project& project)
    {
        return {
            { "name", project.name },
            { "engine", "MiniEngine" },
            { "engineVersion", "0.1.0" },
            { "assetDir", ToStoredPath(project.assetPath, project.rootPath) },
            { "sceneDir", ToStoredPath(project.scenePath.parent_path(), project.rootPath) },
            { "startupScene", ToStoredPath(project.startupScenePath, project.rootPath) },
            { "configDir", ToStoredPath(project.configPath.parent_path(), project.rootPath) },
            { "buildOutputDir", ToStoredPath(project.buildOutputPath, project.rootPath) },
            { "runtimeId", project.runtimeIdentifier }
        };
    }

    void SaveProjectFile(const Project& project)
    {
        if (project.projectFile.empty())
        {
            return;
        }

        std::ofstream projectFile(project.projectFile);
        if (projectFile.is_open())
        {
            projectFile << BuildProjectJson(project).dump(4);
        }
    }

    void SaveGameConfig(const Project& project)
    {
        if (project.configPath.empty())
        {
            return;
        }

        std::ofstream configFile(project.configPath);
        if (configFile.is_open())
        {
            configFile << "startup_scene = " << ToStoredPath(project.startupScenePath, project.rootPath) << "\n";
            configFile << "build_output_dir = " << ToStoredPath(project.buildOutputPath, project.rootPath) << "\n";
            configFile << "runtime_id = " << project.runtimeIdentifier << "\n";
        }
    }

    void SaveProjectSettings(const Project& project)
    {
        EngineSettings settings;
        settings.build.runtimeIdentifier = project.runtimeIdentifier;
        settings.build.outputFolder = project.buildOutputPath;
        settings.build.startupScene = project.startupScenePath;
        SettingsManager::Save(project.rootPath / "Config" / "settings.json", settings);
    }
}

void ProjectManager::Create(const std::filesystem::path& rootPath)
{
    std::filesystem::create_directories(rootPath / "Assets");
    std::filesystem::create_directories(rootPath / "Scenes");
    std::filesystem::create_directories(rootPath / "Config");

    s_ActiveProject.name = rootPath.filename().string();
    s_ActiveProject.runtimeIdentifier = s_ActiveProject.name;
    s_ActiveProject.rootPath = rootPath;
    s_ActiveProject.projectFile = rootPath / (s_ActiveProject.name + ".meproj");
    s_ActiveProject.assetPath = rootPath / "Assets";
    s_ActiveProject.scenePath = rootPath / "Scenes" / "Main.scene";
    s_ActiveProject.startupScenePath = s_ActiveProject.scenePath;
    s_ActiveProject.configPath = rootPath / "Config" / "game.cfg";
    s_ActiveProject.buildOutputPath = rootPath / "PackagedBuild";

    SaveProjectFile(s_ActiveProject);
    SaveGameConfig(s_ActiveProject);
    SaveProjectSettings(s_ActiveProject);

    EntityManager entities(1);
    ComponentManager components;
    EntityMeta meta;
    SceneSerializer::Save(s_ActiveProject.scenePath.string(), entities, components, meta);

    s_HasProject = true;
    std::filesystem::current_path(rootPath);
}

bool ProjectManager::Load(const std::filesystem::path& meprojPath)
{
    if (!std::filesystem::exists(meprojPath) ||
        meprojPath.extension() != ".meproj")
    {
        return false;
    }

    const auto root = meprojPath.parent_path();

    if (!std::filesystem::exists(root / "Assets") ||
        !std::filesystem::exists(root / "Scenes"))
    {
        return false;
    }

    s_ActiveProject.projectFile = meprojPath;
    s_ActiveProject.rootPath = root;

    std::ifstream file(meprojPath);
    if (!file.is_open())
    {
        return false;
    }

    json projectJson;
    file >> projectJson;

    s_ActiveProject.name = projectJson.value("name", meprojPath.stem().string());
    s_ActiveProject.runtimeIdentifier = projectJson.value("runtimeId", s_ActiveProject.name);
    s_ActiveProject.assetPath = root / projectJson.value("assetDir", std::string{ "Assets" });
    const std::filesystem::path configDir = root / projectJson.value("configDir", std::string{ "Config" });
    s_ActiveProject.configPath = configDir / "game.cfg";
    if (!std::filesystem::exists(s_ActiveProject.configPath))
    {
        s_ActiveProject.configPath = root / "game.cfg";
    }
    const std::filesystem::path sceneDir = root / projectJson.value("sceneDir", std::string{ "Scenes" });
    s_ActiveProject.scenePath = sceneDir / "Main.scene";
    s_ActiveProject.startupScenePath = root / projectJson.value("startupScene", std::string{ "Scenes/Main.scene" });
    const std::filesystem::path defaultBuildOutput = root / "PackagedBuild";
    s_ActiveProject.buildOutputPath = projectJson.contains("buildOutputDir")
        ? root / projectJson.value("buildOutputDir", std::string{ "PackagedBuild" })
        : defaultBuildOutput;

    EngineSettings settings;
    if (SettingsManager::Load(root / "Config" / "settings.json", settings))
    {
        s_ActiveProject.runtimeIdentifier = settings.build.runtimeIdentifier;
        s_ActiveProject.buildOutputPath = settings.build.outputFolder.is_absolute()
            ? settings.build.outputFolder
            : root / settings.build.outputFolder;
        s_ActiveProject.startupScenePath = settings.build.startupScene.is_absolute()
            ? settings.build.startupScene
            : root / settings.build.startupScene;
    }

    s_HasProject = true;
    std::filesystem::current_path(root);
    return true;
}

void ProjectManager::LoadRuntimeLayout(
    const std::string& projectName,
    const std::filesystem::path& runtimeRoot,
    const std::filesystem::path& assetDir,
    const std::filesystem::path& startupScene,
    const std::filesystem::path& configPath)
{
    s_ActiveProject.name = projectName.empty() ? runtimeRoot.filename().string() : projectName;
    s_ActiveProject.runtimeIdentifier = s_ActiveProject.name;
    s_ActiveProject.rootPath = runtimeRoot;
    s_ActiveProject.projectFile.clear();
    s_ActiveProject.assetPath = assetDir.is_absolute() ? assetDir : runtimeRoot / assetDir;
    s_ActiveProject.scenePath = startupScene.is_absolute() ? startupScene : runtimeRoot / startupScene;
    s_ActiveProject.startupScenePath = s_ActiveProject.scenePath;
    s_ActiveProject.configPath = configPath.is_absolute() ? configPath : runtimeRoot / configPath;
    s_ActiveProject.buildOutputPath = runtimeRoot;

    s_HasProject = true;
    std::filesystem::current_path(runtimeRoot);
}


bool ProjectManager::HasActiveProject()
{
    return s_HasProject;
}

const Project& ProjectManager::GetActive()
{
    return s_ActiveProject;
}

void ProjectManager::SetStartupScene(const std::filesystem::path& scenePath)
{
    if (!s_HasProject)
    {
        return;
    }

    s_ActiveProject.startupScenePath = scenePath;
    SaveProjectFile(s_ActiveProject);
    SaveGameConfig(s_ActiveProject);
    SaveProjectSettings(s_ActiveProject);
}

void ProjectManager::UpdateBuildSettings(
    const std::filesystem::path& startupScene,
    const std::filesystem::path& buildOutputPath,
    const std::string& runtimeIdentifier)
{
    if (!s_HasProject)
    {
        return;
    }

    if (!startupScene.empty())
    {
        s_ActiveProject.startupScenePath = startupScene;
    }
    if (!buildOutputPath.empty())
    {
        s_ActiveProject.buildOutputPath = buildOutputPath;
    }
    if (!runtimeIdentifier.empty())
    {
        s_ActiveProject.runtimeIdentifier = runtimeIdentifier;
    }

    SaveProjectFile(s_ActiveProject);
    SaveGameConfig(s_ActiveProject);
    SaveProjectSettings(s_ActiveProject);
}
