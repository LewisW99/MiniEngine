#pragma once
#include <string>
#include <vector>
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>
#include <miniaudio.h>

struct ImportedMesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct ImportedTexture {
    int width = 0, height = 0, channels = 0;
    unsigned char* data = nullptr;
};

class AssetImporter {
public:
    static ImportedMesh ImportModel(const std::string& path);
    static void ImportTexture(const std::string& path);
    static void ImportAudio(const std::string& path);
    static void Shutdown(); 
};
