#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <GL/glew.h>


enum class AssetType
{
    Unknown,
    Model,
    Texture,
    Audio,
    Script,
    Prefab,
    Scene
};

struct AssetInfo
{
    GLuint previewID = 0;
    std::string name;
    std::string path;
    AssetType type;

    int width = 0;
    int height = 0;
    int channels = 0;
    bool previewLoaded = false;
};

class AssetDatabase
{
public:
    void Scan(const std::string& directory);
    const std::vector<AssetInfo>& GetAssets() const { return assets; }
    std::vector<AssetInfo>& GetAssets() { return assets; }
    void ClearPreviews();

    inline const char* AssetTypeToString(AssetType type)
    {
        switch (type)
        {
        case AssetType::Model:   return "Model";
        case AssetType::Texture: return "Texture";
        case AssetType::Audio:   return "Audio";
        case AssetType::Script:  return "Script";
        case AssetType::Prefab:  return "Prefab";
        case AssetType::Scene:   return "Scene";
        default:                 return "Unknown";
        }
    }

private:
    std::vector<AssetInfo> assets;
    AssetType DetectType(const std::filesystem::path& ext);
};
