#include "pch.h"
#include "AssetImporter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>

//Audio Engine
static ma_engine gAudioEngine;
static bool gAudioInitialized = false;

static void InitAudio()
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

static void ShutdownAudio()
{
    if (gAudioInitialized)
    {
        ma_engine_uninit(&gAudioEngine);
        gAudioInitialized = false;
        std::cout << "[Audio] MiniAudio shut down.\n";
    }
}

//Model Import
ImportedMesh AssetImporter::ImportModel(const std::string& path)
{
    ImportedMesh mesh;
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_GenNormals |
        aiProcess_ImproveCacheLocality);

    if (!scene || !scene->HasMeshes())
    {
        std::cerr << "[ModelImporter] Failed to load: " << path << "\n";
        return mesh;
    }

    const aiMesh* aiMeshPtr = scene->mMeshes[0];
    mesh.vertices.reserve(aiMeshPtr->mNumVertices * 3);
    mesh.indices.reserve(aiMeshPtr->mNumFaces * 3);

    for (unsigned int i = 0; i < aiMeshPtr->mNumVertices; ++i)
    {
        const aiVector3D& v = aiMeshPtr->mVertices[i];
        mesh.vertices.insert(mesh.vertices.end(), { v.x, v.y, v.z });
    }

    for (unsigned int i = 0; i < aiMeshPtr->mNumFaces; ++i)
    {
        const aiFace& face = aiMeshPtr->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
            mesh.indices.push_back(face.mIndices[j]);
    }

    std::cout << "[ModelImporter] Loaded " << mesh.vertices.size() / 3
        << " vertices, " << mesh.indices.size() / 3 << " triangles from " << path << "\n";
    return mesh;
}

//Import Texture
void AssetImporter::ImportTexture(const std::string& path)
{
    ImportedTexture tex;
    stbi_set_flip_vertically_on_load(true);

    tex.data = stbi_load(path.c_str(), &tex.width, &tex.height, &tex.channels, 0);
    if (!tex.data)
    {
        std::cerr << "[TextureImporter] Failed to load: " << path << "\n";
        return;
    }

    std::cout << "[TextureImporter] Loaded " << path << " ("
        << tex.width << "x" << tex.height << ", " << tex.channels << " channels)\n";

    // --- Copy into project Assets folder ---
    std::filesystem::path src(path);
    std::filesystem::path dst = std::filesystem::current_path() / "Assets" / src.filename();

    std::error_code ec;
    if (!std::filesystem::exists(dst, ec))
        std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);

    if (ec)
        std::cerr << "[AssetImporter] Copy failed: " << ec.message() << "\n";
    else
        std::cout << "[AssetImporter] Imported to " << dst.string() << "\n";

    stbi_image_free(tex.data);
}

//Import Audio
void AssetImporter::ImportAudio(const std::string& path)
{
    InitAudio();

    if (!gAudioInitialized)
    {
        std::cerr << "[AudioImporter] Cannot play audio — engine not initialized.\n";
        return;
    }

    ma_result result = ma_engine_play_sound(&gAudioEngine, path.c_str(), nullptr);
    if (result != MA_SUCCESS)
        std::cerr << "[AudioImporter] Failed to play sound: " << path << "\n";
    else
        std::cout << "[AudioImporter] Playing: " << path << "\n";
}

//CleanShutdown
void AssetImporter::Shutdown()
{
    ShutdownAudio();
}
