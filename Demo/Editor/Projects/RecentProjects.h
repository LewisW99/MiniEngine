#pragma once
#include <vector>
#include <filesystem>

class RecentProjects
{
public:
    static void Load();
    static void Save();

    static void Add(const std::filesystem::path& projectFile);
    static const std::vector<std::filesystem::path>& Get();

private:
    static std::vector<std::filesystem::path> s_Projects;
};
