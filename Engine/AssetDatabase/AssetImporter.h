#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <miniaudio.h>
#include "../Rendering/RenderTypes.h"

struct ImportedTexture
{
    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<std::uint8_t> pixels;

    bool IsValid() const
    {
        return width > 0 && height > 0 && !pixels.empty();
    }
};

class AssetImporter
{
public:
    static MeshData ImportModel(const std::string& path);
    static ImportedTexture LoadTextureData(const std::string& path, bool flipVertically);
    static void ImportTexture(const std::string& path);
    static void ImportAudio(const std::string& path);
    static void Shutdown();
};
