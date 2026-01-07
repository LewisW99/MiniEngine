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

    GLuint LoadTextureForPreview(AssetInfo& a);
};