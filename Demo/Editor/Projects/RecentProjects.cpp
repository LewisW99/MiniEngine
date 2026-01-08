#include "RecentProjects.h"
#include <fstream>
#include <string>
#include <algorithm>

std::vector<std::filesystem::path> RecentProjects::s_Projects;

static std::filesystem::path GetRecentFilePath()
{
    return std::filesystem::current_path() / "recent_projects.txt";
}

void RecentProjects::Load()
{
    s_Projects.clear();
    std::ifstream in(GetRecentFilePath());
    std::string line;

    while (std::getline(in, line))
        s_Projects.emplace_back(line);
}

void RecentProjects::Save()
{
    std::ofstream out(GetRecentFilePath());
    for (auto& p : s_Projects)
        out << p.string() << "\n";
}

void RecentProjects::Add(const std::filesystem::path& projectFile)
{
    s_Projects.erase(
        std::remove(s_Projects.begin(), s_Projects.end(), projectFile),
        s_Projects.end());

    s_Projects.insert(s_Projects.begin(), projectFile);

    if (s_Projects.size() > 10)
        s_Projects.resize(10);

    Save();
}

const std::vector<std::filesystem::path>& RecentProjects::Get()
{
    return s_Projects;
}
