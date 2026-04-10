#include "PrototypeBuilder.h"

#include <filesystem>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
#include "BuildPipeline.h"
#include "Editor/Managers/ProjectManager.h"

namespace
{
    void SetError(std::string* errorOut, const std::string& message)
    {
        if (errorOut != nullptr)
        {
            *errorOut = message;
        }
    }

    void AppendReport(std::string* errorOut, const std::string& message)
    {
        if (errorOut == nullptr || message.empty())
        {
            return;
        }

        if (!errorOut->empty())
        {
            *errorOut += "\n";
        }

        *errorOut += message;
    }
}

bool PrototypeBuilder::Build(const std::filesystem::path& projectFile, std::string* errorOut)
{
    if (!ProjectManager::Load(projectFile))
    {
        SetError(errorOut, "Failed to load project: " + projectFile.string());
        return false;
    }

    const auto& project = ProjectManager::GetActive();
    const std::filesystem::path outputRoot = project.buildOutputPath.is_absolute()
        ? project.buildOutputPath
        : project.rootPath / project.buildOutputPath;

    char modulePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    const std::filesystem::path executablePath = modulePath;
    const std::filesystem::path executableDir = executablePath.parent_path();

    std::error_code ec;
    bool buildSucceeded = true;
    std::filesystem::remove_all(outputRoot, ec);
    ec.clear();
    std::filesystem::create_directories(outputRoot, ec);
    if (ec)
    {
        SetError(errorOut, "Failed to create package folder: " + outputRoot.string());
        return false;
    }

    auto copyIfExists = [&](const std::filesystem::path& source, const std::filesystem::path& dest, const bool required = false)
        {
            if (!std::filesystem::exists(source))
            {
                const std::string message = "[Packaging] Missing " + std::string(required ? "required" : "optional") +
                    " path: " + source.string();
                if (required)
                {
                    SetError(errorOut, message);
                    buildSucceeded = false;
                }
                else
                {
                    AppendReport(errorOut, message);
                }
                return;
            }

            std::filesystem::create_directories(dest.parent_path(), ec);
            ec.clear();
            std::filesystem::copy(source,
                dest,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
                ec);
            if (ec)
            {
                const std::string message = "[Packaging] Copy failed from " + source.string() + " to " + dest.string() +
                    ": " + ec.message();
                if (required)
                {
                    SetError(errorOut, message);
                    buildSucceeded = false;
                }
                else
                {
                    AppendReport(errorOut, message);
                }
            }
        };

    const std::filesystem::path gameExePath = outputRoot / "Game.exe";
    copyIfExists(executableDir / "Demo.exe", gameExePath, true);
    if (!std::filesystem::exists(gameExePath))
    {
        return false;
    }
    copyIfExists(executableDir / "SDL2.dll", outputRoot / "SDL2.dll", true);

    for (const auto& entry : std::filesystem::directory_iterator(executableDir, ec))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".dll")
        {
            continue;
        }

        copyIfExists(entry.path(), outputRoot / entry.path().filename());
    }

    copyIfExists(project.rootPath / "Assets", outputRoot / "Assets", true);
    copyIfExists(project.rootPath / "Scenes", outputRoot / "Scenes", true);
    copyIfExists(project.rootPath / "Scripts", outputRoot / "Scripts");
    copyIfExists(project.rootPath / "Config", outputRoot / "Config", true);

    if (!std::filesystem::exists(outputRoot / "Config" / "game.cfg") && std::filesystem::exists(project.configPath))
    {
        copyIfExists(project.configPath, outputRoot / "Config" / "game.cfg");
    }

    std::ofstream runtimeConfig(outputRoot / "Config" / "runtime.cfg");
    if (runtimeConfig.is_open())
    {
        runtimeConfig << "game_id = " << project.runtimeIdentifier << "\n";
        runtimeConfig << "startup_scene = "
            << project.startupScenePath.lexically_relative(project.rootPath).generic_string() << "\n";
        runtimeConfig << "asset_dir = Assets\n";
        runtimeConfig << "scene_dir = Scenes\n";
        runtimeConfig << "config_dir = Config\n";
    }

    BuildPipeline::WriteRuntimeManifest(project, outputRoot);

    return buildSucceeded && std::filesystem::exists(gameExePath);
}
