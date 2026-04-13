#include "ProjectManager.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unordered_map>
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
    std::string SanitizeSceneName(std::string sceneName)
    {
        for (char& ch : sceneName)
        {
            const bool valid = std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == ' ';
            if (!valid)
            {
                ch = '_';
            }
        }

        sceneName.erase(0, sceneName.find_first_not_of(' '));
        sceneName.erase(sceneName.find_last_not_of(' ') == std::string::npos ? 0 : sceneName.find_last_not_of(' ') + 1);
        return sceneName.empty() ? "Scene" : sceneName;
    }

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

    std::string GenerateSceneId()
    {
        const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        return "scene_" + std::to_string(now);
    }

    const ProjectSceneInfo* FindSceneByPath(const Project& project, const std::filesystem::path& scenePath)
    {
        std::error_code ec;
        const auto normalizedTarget = std::filesystem::weakly_canonical(scenePath, ec);
        const auto fallbackTarget = scenePath.lexically_normal();

        for (const auto& scene : project.scenes)
        {
            std::error_code sceneEc;
            const auto normalizedScene = std::filesystem::weakly_canonical(scene.path, sceneEc);
            if ((!ec && !sceneEc && normalizedScene == normalizedTarget) ||
                scene.path.lexically_normal() == fallbackTarget)
            {
                return &scene;
            }
        }

        return nullptr;
    }

    void RefreshSceneRegistry(Project& project)
    {
        std::unordered_map<std::string, std::string> existingIdsByPath;
        for (const auto& scene : project.scenes)
        {
            existingIdsByPath[ToStoredPath(scene.path, project.rootPath)] = scene.id;
        }

        std::unordered_map<std::string, BuildSceneInfo> existingBuildInfo;
        for (const auto& buildScene : project.buildScenes)
        {
            existingBuildInfo[buildScene.sceneId] = buildScene;
        }

        std::vector<ProjectSceneInfo> refreshedScenes;
        std::error_code ec;
        if (std::filesystem::exists(project.sceneDirectory, ec))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(project.sceneDirectory, ec))
            {
                if (ec || !entry.is_regular_file() || entry.path().extension() != ".scene")
                {
                    continue;
                }

                ProjectSceneInfo scene;
                scene.path = entry.path();
                scene.name = entry.path().stem().string();

                const std::string storedPath = ToStoredPath(scene.path, project.rootPath);
                const auto existingId = existingIdsByPath.find(storedPath);
                scene.id = existingId != existingIdsByPath.end() ? existingId->second : GenerateSceneId();
                refreshedScenes.push_back(scene);
            }
        }

        std::sort(refreshedScenes.begin(), refreshedScenes.end(), [](const ProjectSceneInfo& lhs, const ProjectSceneInfo& rhs)
            {
                return lhs.path.generic_string() < rhs.path.generic_string();
            });

        project.scenes = refreshedScenes;

        if ((project.scenePath.empty() || !std::filesystem::exists(project.scenePath)) && !project.scenes.empty())
        {
            project.scenePath = project.scenes.front().path;
        }

        if ((project.startupScenePath.empty() || !std::filesystem::exists(project.startupScenePath)) && !project.scenes.empty())
        {
            project.startupScenePath = project.scenes.front().path;
        }

        if (const auto* startupScene = FindSceneByPath(project, project.startupScenePath))
        {
            project.startupSceneId = startupScene->id;
        }
        else if (!project.scenes.empty())
        {
            project.startupSceneId = project.scenes.front().id;
            project.startupScenePath = project.scenes.front().path;
        }
        else
        {
            project.startupSceneId.clear();
        }

        int nextBuildIndex = 0;
        std::vector<BuildSceneInfo> refreshedBuildScenes;
        for (const auto& scene : project.scenes)
        {
            auto existing = existingBuildInfo.find(scene.id);
            if (existing != existingBuildInfo.end())
            {
                refreshedBuildScenes.push_back(existing->second);
                nextBuildIndex = std::max(nextBuildIndex, existing->second.buildIndex + 1);
            }
            else
            {
                BuildSceneInfo buildScene;
                buildScene.sceneId = scene.id;
                buildScene.included = scene.id == project.startupSceneId;
                buildScene.buildIndex = buildScene.included ? 0 : nextBuildIndex++;
                refreshedBuildScenes.push_back(buildScene);
            }
        }

        project.buildScenes = refreshedBuildScenes;
    }

    json BuildProjectJson(const Project& project)
    {
        json scenes = json::array();
        for (const auto& scene : project.scenes)
        {
            scenes.push_back({
                { "id", scene.id },
                { "name", scene.name },
                { "path", ToStoredPath(scene.path, project.rootPath) }
                });
        }

        json buildScenes = json::array();
        for (const auto& buildScene : project.buildScenes)
        {
            buildScenes.push_back({
                { "sceneId", buildScene.sceneId },
                { "buildIndex", buildScene.buildIndex },
                { "included", buildScene.included }
                });
        }

        return {
            { "name", project.name },
            { "engine", "MiniEngine" },
            { "engineVersion", "0.1.0" },
            { "assetDir", ToStoredPath(project.assetPath, project.rootPath) },
            { "sceneDir", ToStoredPath(project.sceneDirectory, project.rootPath) },
            { "activeScene", ToStoredPath(project.scenePath, project.rootPath) },
            { "startupScene", ToStoredPath(project.startupScenePath, project.rootPath) },
            { "configDir", ToStoredPath(project.configPath.parent_path(), project.rootPath) },
            { "buildOutputDir", ToStoredPath(project.buildOutputPath, project.rootPath) },
            { "runtimeId", project.runtimeIdentifier },
            { "scenes", scenes },
            { "build", {
                { "startupSceneId", project.startupSceneId },
                { "scenes", buildScenes }
            } }
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

void ProjectManager::Create(const std::filesystem::path& rootPath, const ProjectCreateOptions& options)
{
    std::filesystem::create_directories(rootPath / "Assets");
    std::filesystem::create_directories(rootPath / "Scenes");
    std::filesystem::create_directories(rootPath / "Config");

    s_ActiveProject.name = rootPath.filename().string();
    s_ActiveProject.runtimeIdentifier = s_ActiveProject.name;
    s_ActiveProject.rootPath = rootPath;
    s_ActiveProject.projectFile = rootPath / (s_ActiveProject.name + ".meproj");
    s_ActiveProject.assetPath = rootPath / "Assets";
    s_ActiveProject.sceneDirectory = rootPath / "Scenes";
    s_ActiveProject.scenePath = s_ActiveProject.sceneDirectory / (SanitizeSceneName(options.initialSceneName) + ".scene");
    s_ActiveProject.startupScenePath = s_ActiveProject.scenePath;
    s_ActiveProject.configPath = rootPath / "Config" / "game.cfg";
    s_ActiveProject.buildOutputPath = rootPath / "PackagedBuild";
    s_ActiveProject.scenes.clear();
    s_ActiveProject.buildScenes.clear();
    s_ActiveProject.startupSceneId.clear();

    EntityManager entities(1);
    ComponentManager components;
    EntityMeta meta;
    SceneSerializer::RegisterSceneComponentTypes(components);
    SceneSerializer::Save(s_ActiveProject.scenePath.string(), entities, components, meta);

    s_HasProject = true;
    RefreshSceneRegistry(s_ActiveProject);

    SaveProjectFile(s_ActiveProject);
    SaveGameConfig(s_ActiveProject);
    SaveProjectSettings(s_ActiveProject);
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
    s_ActiveProject.sceneDirectory = sceneDir;
    s_ActiveProject.scenePath = root / projectJson.value("activeScene", std::string{ "Scenes/Main.scene" });
    s_ActiveProject.startupScenePath = root / projectJson.value("startupScene", std::string{ "Scenes/Main.scene" });
    const std::filesystem::path defaultBuildOutput = root / "PackagedBuild";
    s_ActiveProject.buildOutputPath = projectJson.contains("buildOutputDir")
        ? root / projectJson.value("buildOutputDir", std::string{ "PackagedBuild" })
        : defaultBuildOutput;
    s_ActiveProject.scenes.clear();
    s_ActiveProject.buildScenes.clear();
    s_ActiveProject.startupSceneId.clear();

    if (projectJson.contains("scenes"))
    {
        for (const auto& sceneEntry : projectJson["scenes"])
        {
            ProjectSceneInfo scene;
            scene.id = sceneEntry.value("id", GenerateSceneId());
            scene.name = sceneEntry.value("name", std::string{});
            scene.path = root / sceneEntry.value("path", std::string{});
            if (scene.name.empty())
            {
                scene.name = scene.path.stem().string();
            }
            s_ActiveProject.scenes.push_back(scene);
        }
    }

    if (projectJson.contains("build"))
    {
        const auto& buildEntry = projectJson["build"];
        s_ActiveProject.startupSceneId = buildEntry.value("startupSceneId", std::string{});
        if (buildEntry.contains("scenes"))
        {
            for (const auto& buildSceneEntry : buildEntry["scenes"])
            {
                BuildSceneInfo buildScene;
                buildScene.sceneId = buildSceneEntry.value("sceneId", std::string{});
                buildScene.buildIndex = buildSceneEntry.value("buildIndex", buildScene.buildIndex);
                buildScene.included = buildSceneEntry.value("included", buildScene.included);
                s_ActiveProject.buildScenes.push_back(buildScene);
            }
        }
    }

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

    RefreshSceneRegistry(s_ActiveProject);
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
    s_ActiveProject.sceneDirectory = runtimeRoot / "Scenes";
    s_ActiveProject.scenePath = startupScene.is_absolute() ? startupScene : runtimeRoot / startupScene;
    s_ActiveProject.startupScenePath = s_ActiveProject.scenePath;
    s_ActiveProject.configPath = configPath.is_absolute() ? configPath : runtimeRoot / configPath;
    s_ActiveProject.buildOutputPath = runtimeRoot;
    s_ActiveProject.scenes.clear();
    s_ActiveProject.buildScenes.clear();
    s_ActiveProject.startupSceneId.clear();

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

const std::vector<ProjectSceneInfo>& ProjectManager::GetScenes()
{
    return s_ActiveProject.scenes;
}

const std::vector<BuildSceneInfo>& ProjectManager::GetBuildScenes()
{
    return s_ActiveProject.buildScenes;
}

bool ProjectManager::CreateScene(const std::string& sceneName, std::filesystem::path* createdScenePath)
{
    if (!s_HasProject)
    {
        return false;
    }

    const std::string sanitizedName = SanitizeSceneName(sceneName);
    std::filesystem::path scenePath = s_ActiveProject.sceneDirectory / (sanitizedName + ".scene");
    int suffix = 1;
    while (std::filesystem::exists(scenePath))
    {
        scenePath = s_ActiveProject.sceneDirectory / (sanitizedName + "_" + std::to_string(suffix++) + ".scene");
    }

    EntityManager entities(1);
    ComponentManager components;
    EntityMeta meta;
    SceneSerializer::RegisterSceneComponentTypes(components);
    SceneSerializer::Save(scenePath.string(), entities, components, meta);

    s_ActiveProject.scenePath = scenePath;
    if (s_ActiveProject.startupScenePath.empty())
    {
        s_ActiveProject.startupScenePath = scenePath;
    }

    RefreshSceneRegistry(s_ActiveProject);
    SaveProjectFile(s_ActiveProject);
    SaveGameConfig(s_ActiveProject);
    SaveProjectSettings(s_ActiveProject);

    if (createdScenePath != nullptr)
    {
        *createdScenePath = scenePath;
    }

    return true;
}

bool ProjectManager::OpenScene(const std::filesystem::path& scenePath)
{
    if (!s_HasProject || scenePath.empty() || !std::filesystem::exists(scenePath))
    {
        return false;
    }

    s_ActiveProject.scenePath = scenePath.is_absolute() ? scenePath : s_ActiveProject.rootPath / scenePath;
    RefreshSceneRegistry(s_ActiveProject);
    SaveProjectFile(s_ActiveProject);
    return true;
}

void ProjectManager::SetStartupScene(const std::filesystem::path& scenePath)
{
    if (!s_HasProject)
    {
        return;
    }

    s_ActiveProject.startupScenePath = scenePath;
    if (const auto* scene = FindSceneByPath(s_ActiveProject, scenePath))
    {
        s_ActiveProject.startupSceneId = scene->id;
    }
    SaveProjectFile(s_ActiveProject);
    SaveGameConfig(s_ActiveProject);
    SaveProjectSettings(s_ActiveProject);
}

void ProjectManager::UpdateBuildSettings(const std::filesystem::path& buildOutputPath,
    const std::string& runtimeIdentifier,
    const std::string& startupSceneId,
    const std::vector<BuildSceneInfo>& buildScenes)
{
    if (!s_HasProject)
    {
        return;
    }

    if (!buildOutputPath.empty())
    {
        s_ActiveProject.buildOutputPath = buildOutputPath;
    }
    if (!runtimeIdentifier.empty())
    {
        s_ActiveProject.runtimeIdentifier = runtimeIdentifier;
    }
    if (!startupSceneId.empty())
    {
        s_ActiveProject.startupSceneId = startupSceneId;
        for (const auto& scene : s_ActiveProject.scenes)
        {
            if (scene.id == startupSceneId)
            {
                s_ActiveProject.startupScenePath = scene.path;
                break;
            }
        }
    }
    if (!buildScenes.empty())
    {
        s_ActiveProject.buildScenes = buildScenes;
    }

    SaveProjectFile(s_ActiveProject);
    SaveGameConfig(s_ActiveProject);
    SaveProjectSettings(s_ActiveProject);
}
