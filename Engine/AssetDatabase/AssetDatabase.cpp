#include "pch.h"
#include "AssetDatabase.h"
#include <algorithm>
#include "stb_image.h"
#include <iostream>

void AssetDatabase::Scan(const std::string& directory)
{
    assets.clear();

    const std::filesystem::path scanRoot = directory;
    if (projectRoot.empty())
    {
        projectRoot = scanRoot;
    }

    std::error_code ec;
    if (!std::filesystem::exists(scanRoot, ec))
    {
        std::filesystem::create_directories(scanRoot, ec);
        if (ec)
        {
            std::cerr << "[AssetDB] Failed to create or access directory: " << directory << "\n";
            return;
        }
    }

    for (auto& entry : std::filesystem::recursive_directory_iterator(scanRoot, ec))
    {
        if (ec) {
            std::cerr << "[AssetDB] Error reading directory: " << ec.message() << "\n";
            return;
        }

        if (!entry.is_regular_file()) continue;

        AssetInfo info;
        std::error_code relativeEc;
        const auto relativePath = std::filesystem::relative(entry.path(), projectRoot, relativeEc);
        info.path = relativeEc ? entry.path().string() : relativePath.generic_string();
        info.name = entry.path().filename().string();
        info.type = DetectType(entry.path().extension());

        if (info.type == AssetType::Unknown)
        {
            continue;
        }

        if (info.type == AssetType::Texture)
        {
            int w, h, c;
            unsigned char* data = stbi_load(entry.path().string().c_str(), &w, &h, &c, 0);
            if (data)
            {
                info.width = w;
                info.height = h;
                info.channels = c;
                stbi_image_free(data);
            }
        }

        assets.push_back(info);
    }

    std::sort(assets.begin(), assets.end(), [](const AssetInfo& lhs, const AssetInfo& rhs)
        {
            if (lhs.type != rhs.type)
            {
                return lhs.type < rhs.type;
            }

            return lhs.name < rhs.name;
        });
}

void AssetDatabase::ClearPreviews()
{
    for (auto& a : assets)
    {
        a.previewID = 0;
        a.previewLoaded = false;
    }
}


AssetType AssetDatabase::DetectType(const std::filesystem::path& ext)
{
    std::string e = ext.string();
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);

    if (e == ".fbx" || e == ".obj" || e == ".gltf" || e == ".glb" || e == ".dae" || e == ".3ds" || e == ".blend" || e == ".ply" || e == ".stl") return AssetType::Model;
    if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".hdr" || e == ".psd") return AssetType::Texture;
    if (e == ".wav" || e == ".mp3" || e == ".ogg" || e == ".flac") return AssetType::Audio;
    if (e == ".lua" || e == ".cs" || e == ".cpp" || e == ".h") return AssetType::Script;
    if (e == ".prefab") return AssetType::Prefab;
    if (e == ".scene" || e == ".json") return AssetType::Scene;

    return AssetType::Unknown;
}
