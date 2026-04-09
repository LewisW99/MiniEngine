#include "pch.h"
#include "AssetImporter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>
#include <iostream>

namespace
{
    ma_engine gAudioEngine;
    bool gAudioInitialized = false;

    void InitAudio()
    {
        if (!gAudioInitialized)
        {
            if (ma_engine_init(nullptr, &gAudioEngine) == MA_SUCCESS)
            {
                gAudioInitialized = true;
                std::cout << "[Audio] MiniAudio initialized.\n";
            }
            else
            {
                std::cerr << "[Audio] Failed to initialize MiniAudio engine.\n";
            }
        }
    }

    void ShutdownAudio()
    {
        if (gAudioInitialized)
        {
            ma_engine_uninit(&gAudioEngine);
            gAudioInitialized = false;
            std::cout << "[Audio] MiniAudio shut down.\n";
        }
    }
}

MeshData AssetImporter::ImportModel(const std::string& path)
{
    MeshData mesh;
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_GenNormals |
        aiProcess_ImproveCacheLocality |
        aiProcess_FlipUVs);

    if (scene == nullptr || !scene->HasMeshes())
    {
        std::cerr << "[ModelImporter] Failed to load: " << path << "\n";
        return mesh;
    }

    const aiMesh* sourceMesh = scene->mMeshes[0];
    mesh.vertices.reserve(sourceMesh->mNumVertices);
    mesh.indices.reserve(sourceMesh->mNumFaces * 3);

    for (unsigned int i = 0; i < sourceMesh->mNumVertices; ++i)
    {
        Vertex vertex{};

        if (sourceMesh->HasPositions())
        {
            const aiVector3D& position = sourceMesh->mVertices[i];
            vertex.position = { position.x, position.y, position.z };
        }

        if (sourceMesh->HasNormals())
        {
            const aiVector3D& normal = sourceMesh->mNormals[i];
            vertex.normal = { normal.x, normal.y, normal.z };
        }

        if (sourceMesh->HasTextureCoords(0))
        {
            const aiVector3D& uv = sourceMesh->mTextureCoords[0][i];
            vertex.uv = { uv.x, uv.y };
        }

        mesh.vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < sourceMesh->mNumFaces; ++i)
    {
        const aiFace& face = sourceMesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        {
            mesh.indices.push_back(face.mIndices[j]);
        }
    }

    std::cout << "[ModelImporter] Loaded "
        << mesh.vertices.size()
        << " vertices, "
        << mesh.indices.size() / 3
        << " triangles from "
        << path
        << "\n";

    return mesh;
}

ImportedTexture AssetImporter::LoadTextureData(const std::string& path, const bool flipVertically)
{
    ImportedTexture texture;
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    unsigned char* pixels = stbi_load(path.c_str(), &texture.width, &texture.height, &texture.channels, 0);
    if (pixels == nullptr)
    {
        std::cerr << "[TextureImporter] Failed to load: " << path << "\n";
        return texture;
    }

    const std::size_t byteCount = static_cast<std::size_t>(texture.width) *
        static_cast<std::size_t>(texture.height) *
        static_cast<std::size_t>(texture.channels);

    texture.pixels.assign(pixels, pixels + byteCount);
    stbi_image_free(pixels);
    return texture;
}

void AssetImporter::ImportTexture(const std::string& path)
{
    const ImportedTexture texture = LoadTextureData(path, true);
    if (!texture.IsValid())
    {
        return;
    }

    std::cout << "[TextureImporter] Loaded " << path << " ("
        << texture.width << "x" << texture.height << ", "
        << texture.channels << " channels)\n";

    const std::filesystem::path src(path);
    const std::filesystem::path dst = std::filesystem::current_path() / "Assets" / src.filename();

    std::error_code ec;
    if (!std::filesystem::exists(dst, ec))
    {
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    }

    if (ec)
    {
        std::cerr << "[AssetImporter] Copy failed: " << ec.message() << "\n";
    }
    else
    {
        std::cout << "[AssetImporter] Imported to " << dst.string() << "\n";
    }
}

void AssetImporter::ImportAudio(const std::string& path)
{
    InitAudio();

    if (!gAudioInitialized)
    {
        std::cerr << "[AudioImporter] Cannot play audio - engine not initialized.\n";
        return;
    }

    const ma_result result = ma_engine_play_sound(&gAudioEngine, path.c_str(), nullptr);
    if (result != MA_SUCCESS)
    {
        std::cerr << "[AudioImporter] Failed to play sound: " << path << "\n";
    }
    else
    {
        std::cout << "[AudioImporter] Playing: " << path << "\n";
    }
}

void AssetImporter::Shutdown()
{
    ShutdownAudio();
}
