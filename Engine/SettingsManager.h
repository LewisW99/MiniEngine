#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

struct RuntimeSettings
{
    float gamma = 2.2f;
    float exposure = 1.0f;
    bool vignette = true;
    float masterVolume = 1.0f;
};

struct BuildSettings
{
    std::string runtimeIdentifier{ "MiniEngineGame" };
    std::filesystem::path outputFolder{ "PackagedBuild" };
    std::filesystem::path startupScene{ "Scenes/Main.scene" };
};

struct InputSettings
{
    std::filesystem::path bindingsFile{ "Config/input_bindings.json" };
};

struct EngineSettings
{
    RuntimeSettings runtime;
    BuildSettings build;
    InputSettings input;
};

class SettingsManager
{
public:
    static bool Load(const std::filesystem::path& path, EngineSettings& settings)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        nlohmann::json root;
        file >> root;
        settings.runtime.gamma = root["runtime"].value("gamma", settings.runtime.gamma);
        settings.runtime.exposure = root["runtime"].value("exposure", settings.runtime.exposure);
        settings.runtime.vignette = root["runtime"].value("vignette", settings.runtime.vignette);
        settings.runtime.masterVolume = root["runtime"].value("masterVolume", settings.runtime.masterVolume);
        settings.build.runtimeIdentifier = root["build"].value("runtimeIdentifier", settings.build.runtimeIdentifier);
        settings.build.outputFolder = root["build"].value("outputFolder", settings.build.outputFolder.generic_string());
        settings.build.startupScene = root["build"].value("startupScene", settings.build.startupScene.generic_string());
        settings.input.bindingsFile = root["input"].value("bindingsFile", settings.input.bindingsFile.generic_string());
        return true;
    }

    static bool Save(const std::filesystem::path& path, const EngineSettings& settings)
    {
        nlohmann::json root = {
            { "runtime", {
                { "gamma", settings.runtime.gamma },
                { "exposure", settings.runtime.exposure },
                { "vignette", settings.runtime.vignette },
                { "masterVolume", settings.runtime.masterVolume }
            } },
            { "build", {
                { "runtimeIdentifier", settings.build.runtimeIdentifier },
                { "outputFolder", settings.build.outputFolder.generic_string() },
                { "startupScene", settings.build.startupScene.generic_string() }
            } },
            { "input", {
                { "bindingsFile", settings.input.bindingsFile.generic_string() }
            } }
        };

        std::ofstream file(path);
        if (!file.is_open())
        {
            return false;
        }

        file << root.dump(4);
        return true;
    }
};
