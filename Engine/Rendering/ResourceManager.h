#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "RenderTypes.h"

class ResourceManager
{
public:
    static const Mesh* LoadMesh(const std::string& path);
    static const Mesh* GetMesh(const std::string& path);

    static GLuint LoadTexture(const std::string& path);
    static GLuint GetTexture(const std::string& path);

    static void Clear();

private:
    static std::string NormalizePath(const std::string& path);
    static MeshData CreateBuiltinCube();
    static Mesh UploadMesh(const MeshData& meshData);

    static std::unordered_map<std::string, Mesh> s_Meshes;
    static std::unordered_map<std::string, GLuint> s_Textures;
};
