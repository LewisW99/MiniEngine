#include "pch.h"
#include "ResourceManager.h"

#include <cstddef>
#include <filesystem>
#include <iostream>
#include "../AssetDatabase/AssetImporter.h"
#include "../../Demo/Editor/Managers/ProjectManager.h"

std::unordered_map<std::string, Mesh> ResourceManager::s_Meshes;
std::unordered_map<std::string, GLuint> ResourceManager::s_Textures;
std::unordered_set<std::string> ResourceManager::s_FailedMeshes;

const Mesh* ResourceManager::LoadMesh(const std::string& path)
{
    const std::string key = NormalizePath(path);
    if (const auto failed = s_FailedMeshes.find(key); failed != s_FailedMeshes.end())
    {
        return nullptr;
    }

    if (const auto it = s_Meshes.find(key); it != s_Meshes.end())
    {
        return &it->second;
    }

    const MeshData meshData = key == "builtin://cube"
        ? CreateBuiltinCube()
        : AssetImporter::ImportModel(key);

    if (!meshData.IsValid())
    {
        if (key == "builtin://cube")
        {
            std::cerr << "[ResourceManager] Failed to create builtin cube mesh.\n";
        }
        else
        {
            std::cerr << "[ResourceManager] Failed to load mesh resource: " << key;
            if (!AssetImporter::GetLastError().empty())
            {
                std::cerr << " | " << AssetImporter::GetLastError();
            }
            std::cerr << "\n";
        }

        s_FailedMeshes.insert(key);
        return nullptr;
    }

    auto [it, inserted] = s_Meshes.emplace(key, UploadMesh(meshData));
    if (!inserted || !it->second.IsValid())
    {
        std::cerr << "[ResourceManager] Failed to upload mesh resource: " << key << "\n";
        s_FailedMeshes.insert(key);
        return nullptr;
    }

    s_FailedMeshes.erase(key);
    return &it->second;
}

const Mesh* ResourceManager::GetMesh(const std::string& path)
{
    const std::string key = NormalizePath(path);
    if (const auto it = s_Meshes.find(key); it != s_Meshes.end())
    {
        return &it->second;
    }

    return LoadMesh(key);
}

GLuint ResourceManager::LoadTexture(const std::string& path)
{
    const std::string key = NormalizePath(path);
    if (key.empty())
    {
        return 0;
    }

    if (const auto it = s_Textures.find(key); it != s_Textures.end())
    {
        return it->second;
    }

    const ImportedTexture texture = AssetImporter::LoadTextureData(key, true);
    if (!texture.IsValid())
    {
        return 0;
    }

    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    const GLenum format = texture.channels == 4 ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D,
        0,
        static_cast<GLint>(format),
        texture.width,
        texture.height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        texture.pixels.data());

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    s_Textures.emplace(key, textureId);
    return textureId;
}

GLuint ResourceManager::GetTexture(const std::string& path)
{
    const std::string key = NormalizePath(path);
    if (const auto it = s_Textures.find(key); it != s_Textures.end())
    {
        return it->second;
    }

    return LoadTexture(key);
}

void ResourceManager::Clear()
{
    for (auto& [path, mesh] : s_Meshes)
    {
        if (mesh.ebo != 0)
        {
            glDeleteBuffers(1, &mesh.ebo);
        }

        if (mesh.vbo != 0)
        {
            glDeleteBuffers(1, &mesh.vbo);
        }

        if (mesh.vao != 0)
        {
            glDeleteVertexArrays(1, &mesh.vao);
        }
    }
    s_Meshes.clear();
    s_FailedMeshes.clear();

    for (auto& [path, texture] : s_Textures)
    {
        if (texture != 0)
        {
            glDeleteTextures(1, &texture);
        }
    }
    s_Textures.clear();
}

std::string ResourceManager::NormalizePath(const std::string& path)
{
    if (path.empty())
    {
        return "builtin://cube";
    }

    if (path.rfind("builtin://", 0) == 0)
    {
        return path;
    }

    std::filesystem::path resolvedPath(path);
    if (!resolvedPath.is_absolute())
    {
        if (ProjectManager::HasActiveProject())
        {
            resolvedPath = ProjectManager::GetActive().rootPath / resolvedPath;
        }
        else
        {
            std::error_code currentPathEc;
            resolvedPath = std::filesystem::current_path(currentPathEc) / resolvedPath;
        }
    }

    std::error_code ec;
    const auto canonicalPath = std::filesystem::weakly_canonical(resolvedPath, ec);
    return ec ? resolvedPath.lexically_normal().generic_string() : canonicalPath.generic_string();
}

MeshData ResourceManager::CreateBuiltinCube()
{
    MeshData mesh;
    mesh.vertices = {
        {{ 0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }},
        {{ 0.5f,  0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }},
        {{ 0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }},
        {{ 0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }},

        {{ -0.5f, -0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f }},
        {{ -0.5f, -0.5f, -0.5f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f }},

        {{ -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }},
        {{ 0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }},
        {{ 0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f }},

        {{ -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }},
        {{ -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f }},
        {{ 0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f }},
        {{ 0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }},

        {{ -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }},
        {{ 0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }},
        {{ 0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }},

        {{ 0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }},
        {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }},
        {{ 0.5f,  0.5f, -0.5f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }}
    };

    mesh.indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    return mesh;
}

Mesh ResourceManager::UploadMesh(const MeshData& meshData)
{
    Mesh mesh;
    mesh.indexCount = static_cast<GLsizei>(meshData.indices.size());

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(meshData.vertices.size() * sizeof(Vertex)),
        meshData.vertices.data(),
        GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(meshData.indices.size() * sizeof(std::uint32_t)),
        meshData.indices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return mesh;
}
