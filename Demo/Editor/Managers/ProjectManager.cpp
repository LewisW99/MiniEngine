#include "ProjectManager.h"
#include <filesystem>
#include <fstream>

Project ProjectManager::s_ActiveProject{};
bool ProjectManager::s_HasProject = false;

void ProjectManager::Create(const std::filesystem::path& rootPath)
{
    std::filesystem::create_directories(rootPath / "Assets");
    std::filesystem::create_directories(rootPath / "Scenes");

    s_ActiveProject.name = rootPath.filename().string();
    s_ActiveProject.rootPath = rootPath;
    s_ActiveProject.projectFile = rootPath / (s_ActiveProject.name + ".meproj");
    s_ActiveProject.assetPath = rootPath / "Assets";
    s_ActiveProject.scenePath = rootPath / "Scenes" / "Main.scene";

    // ---------------- Write .meproj ----------------
    std::ofstream file(s_ActiveProject.projectFile);
    file << "{\n";
    file << "  \"name\": \"" << s_ActiveProject.name << "\",\n";
    file << "  \"engine\": \"MiniEngine\",\n";
    file << "  \"engineVersion\": \"0.1.0\",\n";
    file << "  \"assetDir\": \"Assets\",\n";
    file << "  \"sceneDir\": \"Scenes\"\n";
    file << "}\n";
    file.close();
    // ------------------------------------------------

    s_HasProject = true;
}

bool ProjectManager::Load(const std::filesystem::path& meprojPath)
{
    if (!std::filesystem::exists(meprojPath) ||
        meprojPath.extension() != ".meproj")
    {
        return false;
    }

    auto root = meprojPath.parent_path();

    if (!std::filesystem::exists(root / "Assets") ||
        !std::filesystem::exists(root / "Scenes"))
    {
        return false;
    }

    s_ActiveProject.projectFile = meprojPath;
    s_ActiveProject.rootPath = root;
    s_ActiveProject.name = meprojPath.stem().string();
    s_ActiveProject.assetPath = root / "Assets";
    s_ActiveProject.scenePath = root / "Scenes" / "Main.scene";

    s_HasProject = true;
    return true;
}


bool ProjectManager::HasActiveProject()
{
    return s_HasProject;
}

const Project& ProjectManager::GetActive()
{
    return s_ActiveProject;
}
