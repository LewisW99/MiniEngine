#pragma once
#include "../Engine/ECS/EntityManager.h"
#include "../Engine/ECS/ComponentManager.h"
#include "../Engine/TransformSystem.h"
#include "../Engine/Renderer.h"
#include "../Engine/Rendering/Camera.h"
#include "../Engine/Streaming/StreamingManager.h"
#include "../Engine/ECS/EntityMeta.h"

#include "../Engine/AssetDatabase/AssetDatabase.h"
#include <imgui.h>

enum class AppState {
    Startup,
    Editor
};

enum class EngineMode {
    Editor,
    Play
};

//Manages docking and ECU panels
class Editor
{
public:
    Editor(EntityManager* entityMgr,
        ComponentManager* compMgr,
        Renderer* renderer,
        Camera* camera,
        StreamingManager* streamer);

    void Draw();

    std::string statusText;
    float statusTimer = 0.0f;

	AssetDatabase assetDB;

    const AssetInfo* selectedAsset = nullptr;

    std::filesystem::path activeScenePath;

	EngineMode GetEngineMode() const { return engineMode; }

private:
    EntityManager* entityMgr = nullptr;
    ComponentManager* compMgr = nullptr;
    Renderer* renderer = nullptr;
    Camera* camera = nullptr;             
    StreamingManager* streamer = nullptr;  

    Entity selectedEntity;
    EntityMeta meta;
	Entity renamingEntity;

    void DrawSceneView();
    void DrawHierarchy();
    void DrawDetails();
    void DrawAssetsPanel();
    void BeginDockSpace();

    void TogglePlayMode();

    void EnterPlayMode();
	void ExitPlayMode();

    GLuint LoadTextureForPreview(AssetInfo& a);

	EngineMode engineMode = EngineMode::Editor;
};