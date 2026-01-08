#pragma once
#include <string>
#include <filesystem>


struct Project
{
    std::string name;
    std::filesystem::path rootPath;
    std::filesystem::path projectFile;
    std::filesystem::path assetPath;
    std::filesystem::path scenePath;
};

class ProjectManager
{
public:
    static void Create(const std::filesystem::path& rootPath);
    static bool Load(const std::filesystem::path& rootPath);

    static bool HasActiveProject();
    static const Project& GetActive();

private:
    static Project s_ActiveProject;
    static bool s_HasProject;
};