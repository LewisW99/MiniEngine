#include "pch.h"
#include "AssetImporter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <limits>
#include <sstream>
#include <unordered_set>
#include "../../Demo/Editor/Managers/ProjectManager.h"

namespace
{
    ma_engine gAudioEngine;
    bool gAudioInitialized = false;
    std::string gLastError;
    std::vector<std::string> gDiagnostics;
    const std::unordered_set<std::string> kModelExtensions = {
        ".obj", ".fbx", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl"
    };
    const std::unordered_set<std::string> kTextureExtensions = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".psd"
    };
    const std::unordered_set<std::string> kAudioExtensions = {
        ".wav", ".mp3", ".ogg", ".flac"
    };

    void AddDiagnostic(const std::string& message)
    {
        gDiagnostics.push_back(message);
        if (gDiagnostics.size() > 64)
        {
            gDiagnostics.erase(gDiagnostics.begin(), gDiagnostics.begin() + static_cast<std::ptrdiff_t>(gDiagnostics.size() - 64));
        }
    }

    void SetLastError(const std::string& message)
    {
        gLastError = message;
        AddDiagnostic(message);
        std::cerr << message << "\n";
    }

    std::string ToLowerExtension(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        return extension;
    }

    bool ValidateExtension(
        const std::filesystem::path& path,
        const std::unordered_set<std::string>& supportedExtensions,
        const char* assetKind)
    {
        const std::string extension = ToLowerExtension(path);
        if (supportedExtensions.find(extension) != supportedExtensions.end())
        {
            return true;
        }

        SetLastError(std::string("[AssetImporter] Unsupported ") + assetKind +
            " format: " + path.filename().string());
        return false;
    }

    std::string NormalizeProjectRelativePath(const std::filesystem::path& path)
    {
        const std::filesystem::path projectRoot = ProjectManager::HasActiveProject()
            ? ProjectManager::GetActive().rootPath
            : std::filesystem::current_path();
        std::error_code ec;
        const auto relativePath = std::filesystem::relative(path, projectRoot, ec);
        return ec ? path.generic_string() : relativePath.generic_string();
    }

    std::filesystem::path ResolveProjectPath(const std::string& path)
    {
        const std::filesystem::path source(path);
        if (source.is_absolute())
        {
            return source;
        }

        if (ProjectManager::HasActiveProject())
        {
            return ProjectManager::GetActive().rootPath / source;
        }

        return std::filesystem::current_path() / source;
    }

    std::filesystem::path GetImportTargetFolder(const char* folderName)
    {
        const std::filesystem::path projectRoot = ProjectManager::HasActiveProject()
            ? ProjectManager::GetActive().rootPath
            : std::filesystem::current_path();
        return projectRoot / "Assets" / folderName;
    }

    bool CopyIntoProject(const std::filesystem::path& source, const std::filesystem::path& targetFolder)
    {
        std::error_code ec;
        std::filesystem::create_directories(targetFolder, ec);
        if (ec)
        {
            SetLastError("[AssetImporter] Failed to create folder: " + targetFolder.string());
            return false;
        }

        const auto destination = targetFolder / source.filename();
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
        {
            SetLastError("[AssetImporter] Copy failed: " + ec.message());
            return false;
        }

        return true;
    }

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
    ClearLastError();
    MeshData mesh;
    Assimp::Importer importer;
    const std::filesystem::path resolvedPath = ResolveProjectPath(path);

    const aiScene* scene = importer.ReadFile(resolvedPath.string(),
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType |
        aiProcess_GenNormals |
        aiProcess_ImproveCacheLocality |
        aiProcess_FindInvalidData |
        aiProcess_ValidateDataStructure |
        aiProcess_FlipUVs);

    if (scene == nullptr || !scene->HasMeshes())
    {
        const std::string assimpError = importer.GetErrorString();
        SetLastError("[ModelImporter] Failed to load: " + resolvedPath.string() +
            (assimpError.empty() ? std::string{} : " (" + assimpError + ")"));
        return mesh;
    }

    std::size_t importedMeshCount = 0;
    std::size_t skippedMeshCount = 0;
    std::size_t invalidVertexCount = 0;
    std::size_t invalidFaceCount = 0;

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
    {
        const aiMesh* sourceMesh = scene->mMeshes[meshIndex];
        if (sourceMesh == nullptr || !sourceMesh->HasPositions() || sourceMesh->mNumVertices == 0)
        {
            ++skippedMeshCount;
            continue;
        }

        const std::uint32_t baseVertex = static_cast<std::uint32_t>(mesh.vertices.size());
        mesh.vertices.reserve(mesh.vertices.size() + sourceMesh->mNumVertices);
        mesh.indices.reserve(mesh.indices.size() + sourceMesh->mNumFaces * 3);
        std::vector<std::uint32_t> vertexRemap(
            sourceMesh->mNumVertices,
            (std::numeric_limits<std::uint32_t>::max)());
        bool meshHasValidGeometry = false;

        for (unsigned int i = 0; i < sourceMesh->mNumVertices; ++i)
        {
            Vertex vertex{};
            const aiVector3D& position = sourceMesh->mVertices[i];
            if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
            {
                ++invalidVertexCount;
                continue;
            }

            vertex.position = { position.x, position.y, position.z };

            if (sourceMesh->HasNormals())
            {
                const aiVector3D& normal = sourceMesh->mNormals[i];
                vertex.normal = {
                    std::isfinite(normal.x) ? normal.x : 0.0f,
                    std::isfinite(normal.y) ? normal.y : 1.0f,
                    std::isfinite(normal.z) ? normal.z : 0.0f
                };
            }
            else
            {
                vertex.normal = { 0.0f, 1.0f, 0.0f };
            }

            if (sourceMesh->HasTextureCoords(0))
            {
                const aiVector3D& uv = sourceMesh->mTextureCoords[0][i];
                vertex.uv = {
                    std::isfinite(uv.x) ? uv.x : 0.0f,
                    std::isfinite(uv.y) ? uv.y : 0.0f
                };
            }

            mesh.vertices.push_back(vertex);
            vertexRemap[i] = static_cast<std::uint32_t>(mesh.vertices.size() - baseVertex - 1);
            meshHasValidGeometry = true;
        }

        for (unsigned int i = 0; i < sourceMesh->mNumFaces; ++i)
        {
            const aiFace& face = sourceMesh->mFaces[i];
            if (face.mNumIndices != 3)
            {
                continue;
            }

            bool validFace = true;
            for (unsigned int j = 0; j < face.mNumIndices; ++j)
            {
                if (face.mIndices[j] >= sourceMesh->mNumVertices ||
                    vertexRemap[face.mIndices[j]] == (std::numeric_limits<std::uint32_t>::max)())
                {
                    validFace = false;
                    break;
                }
            }

            if (!validFace)
            {
                ++invalidFaceCount;
                continue;
            }

            for (unsigned int j = 0; j < face.mNumIndices; ++j)
            {
                mesh.indices.push_back(baseVertex + vertexRemap[face.mIndices[j]]);
            }
        }

        if (meshHasValidGeometry)
        {
            ++importedMeshCount;
        }
        else
        {
            ++skippedMeshCount;
        }
    }

    if (!mesh.IsValid())
    {
        std::ostringstream error;
        error << "[ModelImporter] No valid mesh data found: " << resolvedPath.string();
        if (invalidVertexCount > 0 || invalidFaceCount > 0)
        {
            error << " (invalid vertices: " << invalidVertexCount
                << ", invalid faces: " << invalidFaceCount << ")";
        }
        SetLastError(error.str());
        return {};
    }

    std::cout << "[ModelImporter] Loaded "
        << mesh.vertices.size()
        << " vertices, "
        << mesh.indices.size() / 3
        << " triangles across "
        << importedMeshCount
        << " mesh(es) from "
        << NormalizeProjectRelativePath(resolvedPath)
        << "\n";
    AddDiagnostic("[ModelImporter] Loaded " + NormalizeProjectRelativePath(resolvedPath));

    if (skippedMeshCount > 0 || invalidVertexCount > 0 || invalidFaceCount > 0)
    {
        std::cout << "[ModelImporter] Skipped "
            << skippedMeshCount
            << " mesh(es), "
            << invalidVertexCount
            << " invalid vertices, "
            << invalidFaceCount
            << " invalid faces.\n";
    }

    return mesh;
}

bool AssetImporter::ImportModelAsset(const std::string& path)
{
    ClearLastError();
    const std::filesystem::path source = ResolveProjectPath(path);
    if (!std::filesystem::exists(source))
    {
        SetLastError("[AssetImporter] Model source file not found: " + path);
        return false;
    }
    if (!ValidateExtension(source, kModelExtensions, "model"))
    {
        return false;
    }

    const MeshData mesh = ImportModel(path);
    if (!mesh.IsValid())
    {
        return false;
    }

    if (!CopyIntoProject(source, GetImportTargetFolder("Models")))
    {
        return false;
    }

    std::cout << "[AssetImporter] Imported model to "
        << NormalizeProjectRelativePath(GetImportTargetFolder("Models") / source.filename())
        << "\n";
    AddDiagnostic("[AssetImporter] Imported model to " +
        NormalizeProjectRelativePath(GetImportTargetFolder("Models") / source.filename()));
    return true;
}

ImportedTexture AssetImporter::LoadTextureData(const std::string& path, const bool flipVertically)
{
    ClearLastError();
    ImportedTexture texture;
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    const std::filesystem::path resolvedPath = ResolveProjectPath(path);
    if (!std::filesystem::exists(resolvedPath))
    {
        SetLastError("[TextureImporter] Texture source file not found: " + resolvedPath.string());
        return texture;
    }
    if (!ValidateExtension(resolvedPath, kTextureExtensions, "texture"))
    {
        return texture;
    }
    unsigned char* pixels = stbi_load(resolvedPath.string().c_str(), &texture.width, &texture.height, &texture.channels, 0);
    if (pixels == nullptr)
    {
        SetLastError("[TextureImporter] Failed to load: " + resolvedPath.string());
        return texture;
    }
    if (texture.channels < 1 || texture.channels > 4)
    {
        stbi_image_free(pixels);
        SetLastError("[TextureImporter] Unsupported channel layout in: " + resolvedPath.string());
        return {};
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
    AddDiagnostic("[TextureImporter] Loaded " + NormalizeProjectRelativePath(ResolveProjectPath(path)));

    const std::filesystem::path src = ResolveProjectPath(path);
    if (CopyIntoProject(src, GetImportTargetFolder("Textures")))
    {
        std::cout << "[AssetImporter] Imported to "
            << NormalizeProjectRelativePath(GetImportTargetFolder("Textures") / src.filename())
            << "\n";
        AddDiagnostic("[AssetImporter] Imported texture to " +
            NormalizeProjectRelativePath(GetImportTargetFolder("Textures") / src.filename()));
    }
}

void AssetImporter::ImportAudio(const std::string& path)
{
    ClearLastError();
    const std::filesystem::path source = ResolveProjectPath(path);
    if (!std::filesystem::exists(source))
    {
        SetLastError("[AudioImporter] Audio source file not found: " + path);
        return;
    }
    if (!ValidateExtension(source, kAudioExtensions, "audio"))
    {
        return;
    }
    if (!EnsureAudioEngine())
    {
        SetLastError("[AudioImporter] Cannot play audio - engine not initialized.");
        return;
    }

    ma_sound previewSound{};
    const ma_result initResult = ma_sound_init_from_file(
        &gAudioEngine,
        source.string().c_str(),
        MA_SOUND_FLAG_DECODE,
        nullptr,
        nullptr,
        &previewSound);
    if (initResult != MA_SUCCESS)
    {
        SetLastError("[AudioImporter] Failed to decode sound: " + source.string());
        return;
    }
    ma_sound_uninit(&previewSound);

    if (!CopyIntoProject(source, GetImportTargetFolder("Audio")))
    {
        return;
    }

    const ma_result result = ma_engine_play_sound(&gAudioEngine, source.string().c_str(), nullptr);
    if (result != MA_SUCCESS)
    {
        SetLastError("[AudioImporter] Failed to play sound: " + source.string());
    }
    else
    {
        std::cout << "[AudioImporter] Playing: " << NormalizeProjectRelativePath(source) << "\n";
        AddDiagnostic("[AudioImporter] Imported audio " +
            NormalizeProjectRelativePath(GetImportTargetFolder("Audio") / source.filename()));
    }
}

bool AssetImporter::EnsureAudioEngine()
{
    InitAudio();
    return gAudioInitialized;
}

ma_engine* AssetImporter::GetAudioEngine()
{
    return gAudioInitialized ? &gAudioEngine : nullptr;
}

const std::string& AssetImporter::GetLastError()
{
    return gLastError;
}

const std::vector<std::string>& AssetImporter::GetDiagnostics()
{
    return gDiagnostics;
}

void AssetImporter::ClearDiagnostics()
{
    gDiagnostics.clear();
}

void AssetImporter::ClearLastError()
{
    gLastError.clear();
}

void AssetImporter::Shutdown()
{
    ShutdownAudio();
}
