#include "pch.h"
#include "AssetDatabase.h"
#include <algorithm>
#include "stb_image.h"
#include <iostream>

void AssetDatabase::Scan(const std::string& directory)
{
    assets.clear();

    std::error_code ec;
    if (!std::filesystem::exists(directory, ec))
    {
        std::filesystem::create_directories(directory, ec);
        if (ec)
        {
            std::cerr << "[AssetDB] Failed to create or access directory: " << directory << "\n";
            return;
        }
    }

    for (auto& entry : std::filesystem::directory_iterator(directory, ec))
    {
        if (ec) {
            std::cerr << "[AssetDB] Error reading directory: " << ec.message() << "\n";
            return;
        }

        if (!entry.is_regular_file()) continue;

        AssetInfo info;
        info.path = entry.path().string();
        info.name = entry.path().filename().string();
        std::string ext = entry.path().extension().string();

        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb")
            info.type = AssetType::Model;
        else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
        {
            info.type = AssetType::Texture;

            // Read dimensions for textures
            int w, h, c;
            unsigned char* data = stbi_load(info.path.c_str(), &w, &h, &c, 0);
            if (data)
            {
                info.width = w;
                info.height = h;
                info.channels = c;
                stbi_image_free(data);
            }
        }
        else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
            info.type = AssetType::Audio;
        else
            info.type = AssetType::Unknown;

        assets.push_back(info);
    }
}

void AssetDatabase::ClearPreviews()
{
    for (auto& a : assets)
    {
        if (a.previewLoaded && a.previewID != 0)
        {
            glDeleteTextures(1, &a.previewID);
            std::cout << "[AssetDB] Freed preview for " << a.name << " (" << a.previewID << ")\n";
            a.previewID = 0;
            a.previewLoaded = false;
        }
    }
}


AssetType AssetDatabase::DetectType(const std::filesystem::path& ext)
{
    std::string e = ext.string();
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);

    if (e == ".fbx" || e == ".obj" || e == ".gltf" || e == ".glb") return AssetType::Model;
    if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".hdr") return AssetType::Texture;
    if (e == ".wav" || e == ".mp3" || e == ".ogg" || e == ".flac") return AssetType::Audio;
    if (e == ".lua" || e == ".cs" || e == ".cpp" || e == ".h") return AssetType::Script;
    if (e == ".prefab") return AssetType::Prefab;
    if (e == ".scene" || e == ".json") return AssetType::Scene;

    return AssetType::Unknown;
}
