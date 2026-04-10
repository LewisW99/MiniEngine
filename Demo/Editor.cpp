
#include "Editor.h"
#include <cfloat>
#include <cctype>
#include <string>
#include <algorithm>
#include <filesystem>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif
#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include "../Engine/AssetDatabase/AssetImporter.h"
#include "../Engine/Components/AudioSourceComponent.h"
#include "../Engine/Components/AnimationComponent.h"
#include "../Engine/Components/LightComponent.h"
#include "../Engine/Components/MaterialComponent.h"
#include "../Engine/Components/MeshComponent.h"
#include "../Engine/Components/NavAgentComponent.h"
#include "../Engine/Components/NavWaypointComponent.h"
#include "../Engine/Components/RuntimeUIComponent.h"
#include "../Engine/ImGuizmo.h"
#include "../Engine/PrefabSerializer.h"
#include "../Engine/Rendering/ResourceManager.h"
#include "../Engine/SceneSerializer.h"
#include "Editor/Managers/ProjectManager.h"
#include "PrototypeBuilder.h"
#include <sstream>
#include "../Engine/Components/CameraFollowComponent.h"
#include "../Engine/Components/Physics/PhysicsComponent.h"
#include "../Engine/Components/PlayerControllerComponent.h"
#include "../Engine/EditorConsole.h"
#include "../Engine/InputSystem.h"
#include "../Engine/Scripting/ScriptComponent.h"
#include "Scripting/ScriptAPI.h"
#include "../Engine/Renderer.h"

#pragma comment(lib, "Comdlg32.lib")


struct ColliderPreset
{
    const char* name;
    Vec3 halfExtents;
    bool isStatic;
};

static ColliderPreset g_ColliderPresets[] =
{
    { "Player", { 0.4f, 0.9f, 0.4f }, false },
    { "Wall",   { 1.0f, 2.0f, 0.5f }, true  },
    { "Floor",  { 50.0f, 0.5f, 50.0f }, true },
    { "Crate",  { 0.5f, 0.5f, 0.5f }, false }
};

static constexpr int kColliderPresetCount =
sizeof(g_ColliderPresets) / sizeof(ColliderPreset);

namespace
{
    constexpr const char* kAssetPayloadModel = "ASSET_MODEL_PATH";
    constexpr const char* kAssetPayloadTexture = "ASSET_TEXTURE_PATH";
    constexpr const char* kAssetPayloadAudio = "ASSET_AUDIO_PATH";
    constexpr const char* kAssetPayloadScript = "ASSET_SCRIPT_PATH";
    constexpr const char* kAssetPayloadPrefab = "ASSET_PREFAB_PATH";

    const char* GetPayloadType(const AssetType type)
    {
        switch (type)
        {
        case AssetType::Model:
            return kAssetPayloadModel;
        case AssetType::Texture:
            return kAssetPayloadTexture;
        case AssetType::Audio:
            return kAssetPayloadAudio;
        case AssetType::Script:
            return kAssetPayloadScript;
        case AssetType::Prefab:
            return kAssetPayloadPrefab;
        default:
            return nullptr;
        }
    }

    template<typename T>
    void RemoveComponentIfPresent(ComponentManager& components, const Entity entity)
    {
        if (components.HasComponent<T>(entity))
        {
            components.RemoveComponent<T>(entity);
        }
    }

    void RemoveKnownComponents(ComponentManager& components, const Entity entity)
    {
        RemoveComponentIfPresent<TransformComponent>(components, entity);
        RemoveComponentIfPresent<MeshComponent>(components, entity);
        RemoveComponentIfPresent<MaterialComponent>(components, entity);
        RemoveComponentIfPresent<LightComponent>(components, entity);
        RemoveComponentIfPresent<AnimationComponent>(components, entity);
        RemoveComponentIfPresent<PhysicsComponent>(components, entity);
        RemoveComponentIfPresent<ColliderComponent>(components, entity);
        RemoveComponentIfPresent<NavAgentComponent>(components, entity);
        RemoveComponentIfPresent<NavWaypointComponent>(components, entity);
        RemoveComponentIfPresent<RuntimeUIComponent>(components, entity);
        RemoveComponentIfPresent<ScriptComponent>(components, entity);
        RemoveComponentIfPresent<AudioSourceComponent>(components, entity);
        RemoveComponentIfPresent<PlayerControllerComponent>(components, entity);
        RemoveComponentIfPresent<CameraFollowComponent>(components, entity);
    }

    template<typename T>
    void CloneComponentIfPresent(ComponentManager& components, const Entity source, const Entity dest)
    {
        if (components.HasComponent<T>(source))
        {
            components.AddComponent(dest, components.GetComponent<T>(source));
        }
    }

    void CloneKnownComponents(ComponentManager& components, const Entity source, const Entity dest)
    {
        CloneComponentIfPresent<TransformComponent>(components, source, dest);
        CloneComponentIfPresent<MeshComponent>(components, source, dest);
        CloneComponentIfPresent<MaterialComponent>(components, source, dest);
        CloneComponentIfPresent<LightComponent>(components, source, dest);
        CloneComponentIfPresent<AnimationComponent>(components, source, dest);
        CloneComponentIfPresent<PhysicsComponent>(components, source, dest);
        CloneComponentIfPresent<ColliderComponent>(components, source, dest);
        CloneComponentIfPresent<NavAgentComponent>(components, source, dest);
        CloneComponentIfPresent<NavWaypointComponent>(components, source, dest);
        CloneComponentIfPresent<RuntimeUIComponent>(components, source, dest);
        CloneComponentIfPresent<ScriptComponent>(components, source, dest);
        CloneComponentIfPresent<AudioSourceComponent>(components, source, dest);
        CloneComponentIfPresent<PlayerControllerComponent>(components, source, dest);
        CloneComponentIfPresent<CameraFollowComponent>(components, source, dest);
    }
}

static std::string GetLuaTemplateText(
    ScriptTemplate type,
    const std::string& scriptName)
{
    switch (type)
    {
    case ScriptTemplate::PlayerMovement:
        return
            "-- " + scriptName + ".lua\n"
            "-- Player movement example\n\n"
            "local speed = 5.0\n\n"
            "function OnStart(self)\n"
            "    print(\"Player started\")\n"
            "end\n\n"
            "function OnUpdate(self, dt)\n"
            "    Transform.Translate(self, speed * dt, 0, 0)\n"
            "end\n\n"
            "function OnDestroy(self)\n"
            "end\n";

    case ScriptTemplate::CameraFollow:
        return
            "-- " + scriptName + ".lua\n"
            "-- Camera follow example\n\n"
            "local followSpeed = 3.0\n\n"
            "function OnStart(self)\n"
            "    print(\"Camera follow active\")\n"
            "end\n\n"
            "function OnUpdate(self, dt)\n"
            "    Transform.Translate(self, 0, 0, followSpeed * dt)\n"
            "end\n";

    case ScriptTemplate::SimpleAI:
        return
            "-- " + scriptName + ".lua\n"
            "-- Simple AI patrol\n\n"
            "local speed = 2.0\n"
            "local direction = 1\n"
            "local limit = 5.0\n"
            "local traveled = 0.0\n\n"
            "function OnUpdate(self, dt)\n"
            "    local move = direction * speed * dt\n"
            "    Transform.Translate(self, move, 0, 0)\n\n"
            "    traveled = traveled + math.abs(move)\n"
            "    if traveled > limit then\n"
            "        direction = -direction\n"
            "        traveled = 0.0\n"
            "    end\n"
            "end\n";

    case ScriptTemplate::Rotator:
        return
            "-- " + scriptName + ".lua\n"
            "-- Rotating object example\n\n"
            "local rotationSpeed = 45.0\n\n"
            "function OnUpdate(self, dt)\n"
            "    Transform.Rotate(self, 0, rotationSpeed * dt, 0)\n"
            "end\n";

    case ScriptTemplate::BasicMover:
        return
            "-- " + scriptName + ".lua\n"
            "-- Basic mover using Transform.Translate\n\n"
            "local speed = 2.5\n\n"
            "function OnUpdate(self, dt)\n"
            "    Transform.Translate(self, speed * dt, 0, 0)\n"
            "end\n";

    case ScriptTemplate::TriggerInteractable:
        return
            "-- " + scriptName + ".lua\n"
            "-- Trigger/interactable example\n\n"
            "local active = true\n\n"
            "function OnStart(self)\n"
            "    print(\"Interactable ready\")\n"
            "end\n\n"
            "function OnUpdate(self, dt)\n"
            "    if not active then\n"
            "        return\n"
            "    end\n\n"
            "    if Input.Pressed(\"Jump\") then\n"
            "        active = false\n"
            "        print(\"Interaction triggered\")\n"
            "    end\n"
            "end\n";

    case ScriptTemplate::AudioTrigger:
        return
            "-- " + scriptName + ".lua\n"
            "-- Plays an audio source when Jump is pressed\n\n"
            "function OnUpdate(self, dt)\n"
            "    if Input.JumpPressed() then\n"
            "        Audio.Play(self)\n"
            "    end\n"
            "end\n";

    case ScriptTemplate::LightPulse:
        return
            "-- " + scriptName + ".lua\n"
            "-- Pulses a light intensity over time\n\n"
            "local time = 0.0\n\n"
            "function OnUpdate(self, dt)\n"
            "    time = time + dt\n"
            "    local intensity = 1.0 + math.sin(time * 3.0) * 0.5\n"
            "    Light.SetIntensity(self, intensity)\n"
            "end\n";

    case ScriptTemplate::SimplePickup:
        return
            "-- " + scriptName + ".lua\n"
            "-- Simple pickup behaviour\n\n"
            "local spinSpeed = 90.0\n\n"
            "function OnUpdate(self, dt)\n"
            "    Transform.Rotate(self, 0, spinSpeed * dt, 0)\n\n"
            "    if Input.Pressed(\"Jump\") then\n"
            "        Audio.PlayOneShot(\"Assets/Audio/pickup.wav\")\n"
            "        Entity.Destroy(self)\n"
            "    end\n"
            "end\n";

    case ScriptTemplate::Empty:
    default:
        return
            "-- " + scriptName + ".lua\n"
            "-- Docs: https://www.lua.org/manual/5.4/\n\n"
            "function OnStart(self)\n"
            "end\n\n"
            "function OnUpdate(self, dt)\n"
            "end\n\n"
            "function OnDestroy(self)\n"
            "end\n";
    }
}

static bool RayIntersectsAABB(const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax,
    float& tOut)
{
    float tmin = (aabbMin.x - rayOrigin.x) / rayDir.x;
    float tmax = (aabbMax.x - rayOrigin.x) / rayDir.x;
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (aabbMin.y - rayOrigin.y) / rayDir.y;
    float tymax = (aabbMax.y - rayOrigin.y) / rayDir.y;
    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) return false;
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;

    float tzmin = (aabbMin.z - rayOrigin.z) / rayDir.z;
    float tzmax = (aabbMax.z - rayOrigin.z) / rayDir.z;
    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) return false;
    if (tzmin > tmin) tmin = tzmin;
    tOut = tmin;

    return true;
}


static std::string GetCurrentLine(
    const char* buffer,
    int cursorPos)
{
    int start = cursorPos;
    while (start > 0 && buffer[start - 1] != '\n')
        start--;

    int end = cursorPos;
    while (buffer[end] && buffer[end] != '\n')
        end++;

    return std::string(buffer + start, buffer + end);
}

static bool IsValidLuaScriptName(const std::string& name)
{
    if (name.empty())
        return false;

    for (char c : name)
    {
        if (!isalnum(c) && c != '_' && c != '-')
            return false;
    }

    return true;
}

static void InsertTextAtEnd(OpenScript& script, const std::string& text)
{
    // Ensure newline separation
    if (!script.contents.empty() &&
        script.contents.back() != '\n')
    {
        script.contents += "\n";
    }

    script.contents += text;
    script.contents += "\n";

    // Rebuild buffer
    script.buffer.resize(script.contents.size() + 1024);
    memcpy(script.buffer.data(), script.contents.c_str(), script.contents.size());
    script.buffer[script.contents.size()] = '\0';

    script.dirty = true;
}

static std::string ReadFileText(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return "";

    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static void WriteFileText(const std::string& path, const std::string& text)
{
    std::ofstream file(path);
    if (file.is_open())
        file << text;
}

static std::vector<std::string> GetProjectLuaScripts()
{
    std::vector<std::string> scripts;

    if (!ProjectManager::HasActiveProject())
        return scripts;

    const auto& project = ProjectManager::GetActive();
    std::filesystem::path scriptsDir = project.rootPath / "Scripts";

    if (!std::filesystem::exists(scriptsDir))
        return scripts;

    for (auto& entry : std::filesystem::directory_iterator(scriptsDir))
    {
        if (entry.path().extension() == ".lua")
        {
            scripts.push_back(
                "Scripts/" + entry.path().filename().string()
            );
        }
    }

    return scripts;
}



Editor::Editor(EntityManager* entities,
    ComponentManager* components,
    Renderer* renderer,
    Camera* camera,
    StreamingManager* streamer,
    ScriptSystem* scriptSystem,
    InputSystem* inputSystem)
    : entityMgr(entities)
    , compMgr(components)
    , renderer(renderer)
    , camera(camera)
    , streamer(streamer)
    , scriptSystem(scriptSystem)
    , inputSystem(inputSystem)
{
}

bool Editor::LoadActiveProjectScene()
{
    if (!ProjectManager::HasActiveProject())
    {
        return false;
    }

    entityMgr->Clear();
    compMgr->Clear();
    meta.Clear();
    selectedEntity = {};
    renamingEntity = {};

    SceneSerializer::Load(ProjectManager::GetActive().scenePath.string(), *entityMgr, *compMgr, meta);
    EnsureDefaultDirectionalLight();
    SanitizeSelection();
    return true;
}

static int ScriptEditorCallback(ImGuiInputTextCallbackData* data)
{
    Editor* editor = (Editor*)data->UserData;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways)
    {
        editor->scriptCursorPos = data->CursorPos;
    }

    return 0;
}

void Editor::RefreshAssetDatabase()
{
    if (!ProjectManager::HasActiveProject())
    {
        return;
    }

    assetDB.ClearPreviews();
    assetDB.SetRootPath(ProjectManager::GetActive().rootPath);
    assetDB.Scan(ProjectManager::GetActive().rootPath.string());
    selectedAsset = nullptr;
    assetsScanned = true;
}

const AssetInfo* Editor::FindAssetByPath(const std::string& assetPath) const
{
    for (const auto& asset : assetDB.GetAssets())
    {
        if (asset.path == assetPath)
        {
            return &asset;
        }
    }

    return nullptr;
}

bool Editor::AcceptAssetPayload(const char* payloadType, std::string& assetPath) const
{
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType))
    {
        assetPath.assign(static_cast<const char*>(payload->Data), payload->DataSize > 0 ? payload->DataSize - 1 : 0);
        return true;
    }

    return false;
}

bool Editor::PackageProject()
{
    if (!ProjectManager::HasActiveProject())
    {
        return false;
    }

    std::string error;
    if (!PrototypeBuilder::Build(ProjectManager::GetActive().projectFile, &error))
    {
        EditorConsole::Error("[Packaging] " + error);
        return false;
    }

    const auto& project = ProjectManager::GetActive();
    const std::filesystem::path outputRoot = project.buildOutputPath.is_absolute()
        ? project.buildOutputPath
        : project.rootPath / project.buildOutputPath;
    if (!error.empty())
    {
        EditorConsole::Log(error);
    }
    EditorConsole::Log("[Packaging] Wrote prototype build to " + outputRoot.string());
    EditorConsole::Log("[Packaging] Launch packaged runtime with " + (outputRoot / "Game.exe").string());
    return true;
}

bool Editor::SaveSelectedAsPrefab()
{
    if (!selectedEntity || !entityMgr->IsAlive(selectedEntity) || !ProjectManager::HasActiveProject())
    {
        return false;
    }

    const auto& project = ProjectManager::GetActive();
    const std::filesystem::path prefabDir = project.rootPath / "Assets" / "Prefabs";
    std::error_code ec;
    std::filesystem::create_directories(prefabDir, ec);

    std::string prefabName = meta.GetName(selectedEntity);
    if (prefabName.empty())
    {
        prefabName = "Prefab_" + std::to_string(selectedEntity.id);
    }
    std::replace(prefabName.begin(), prefabName.end(), ' ', '_');

    const std::filesystem::path prefabPath = prefabDir / (prefabName + ".prefab");
    if (!PrefabSerializer::Save(prefabPath, selectedEntity, *compMgr, meta))
    {
        EditorConsole::Error("[Prefab] Failed to save prefab: " + prefabPath.string());
        return false;
    }

    RefreshAssetDatabase();
    EditorConsole::Log("[Prefab] Saved prefab: " + prefabPath.string());
    return true;
}

Vec3 Editor::GetSpawnPositionInFrontOfCamera(const float distance) const
{
    if (camera == nullptr)
    {
        return {};
    }

    const glm::vec3 spawnPosition = camera->position + (camera->forward * distance);
    return Vec3(spawnPosition.x, spawnPosition.y, spawnPosition.z);
}

bool Editor::TryGetSceneDropPosition(const ImVec2& imageMin, const ImVec2& imageMax, Vec3& outPosition) const
{
    if (camera == nullptr)
    {
        return false;
    }

    const ImVec2 mousePos = ImGui::GetMousePos();
    if (mousePos.x < imageMin.x || mousePos.x > imageMax.x ||
        mousePos.y < imageMin.y || mousePos.y > imageMax.y)
    {
        return false;
    }

    const ImVec2 imageSize(imageMax.x - imageMin.x, imageMax.y - imageMin.y);
    if (imageSize.x <= 0.0f || imageSize.y <= 0.0f)
    {
        return false;
    }

    const glm::vec2 ndc(
        ((mousePos.x - imageMin.x) / imageSize.x) * 2.0f - 1.0f,
        1.0f - ((mousePos.y - imageMin.y) / imageSize.y) * 2.0f);

    const glm::mat4 proj = glm::perspective(glm::radians(camera->fov), camera->aspect, camera->nearPlane, camera->farPlane);
    const glm::mat4 view = glm::lookAt(camera->position, camera->position + camera->forward, camera->up);
    const glm::mat4 invViewProj = glm::inverse(proj * view);

    glm::vec4 nearWorld = invViewProj * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
    glm::vec4 farWorld = invViewProj * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
    nearWorld /= nearWorld.w;
    farWorld /= farWorld.w;

    const glm::vec3 rayOrigin = glm::vec3(nearWorld);
    const glm::vec3 rayDirection = glm::normalize(glm::vec3(farWorld - nearWorld));
    if (std::abs(rayDirection.y) <= 0.0001f)
    {
        return false;
    }

    const float t = -rayOrigin.y / rayDirection.y;
    if (t <= 0.0f)
    {
        return false;
    }

    const glm::vec3 hitPoint = rayOrigin + rayDirection * t;
    outPosition = Vec3(hitPoint.x, hitPoint.y, hitPoint.z);
    return true;
}

Entity Editor::CreateEntityFromAsset(const AssetInfo& asset, const Vec3* spawnPosition)
{
    Entity entity = entityMgr->CreateEntity();
    TransformComponent transform;
    transform.position = spawnPosition != nullptr ? *spawnPosition : GetSpawnPositionInFrontOfCamera();
    compMgr->AddComponent(entity, transform);
    meta.SetName(entity, std::filesystem::path(asset.path).stem().string());

    switch (asset.type)
    {
    case AssetType::Model:
    {
        MeshComponent mesh;
        mesh.meshPath = asset.path;
        compMgr->AddComponent(entity, mesh);
        compMgr->AddComponent(entity, MaterialComponent{});
        break;
    }
    case AssetType::Script:
    {
        ScriptComponent script;
        script.ScriptPath = asset.path;
        compMgr->AddComponent(entity, script);
        break;
    }
    case AssetType::Prefab:
    {
        const Vec3 resolvedSpawnPosition = spawnPosition != nullptr
            ? *spawnPosition
            : GetSpawnPositionInFrontOfCamera();
        const Entity prefabEntity = PrefabSerializer::Instantiate(asset.path, *entityMgr, *compMgr, meta, &resolvedSpawnPosition);
        if (prefabEntity)
        {
            selectedEntity = prefabEntity;
            return prefabEntity;
        }
        break;
    }
    default:
        break;
    }

    selectedEntity = entity;
    return entity;
}

Entity Editor::CreateDirectionalLightEntity(const Vec3* spawnPosition, const bool selectEntity)
{
    Entity entity = entityMgr->CreateEntity();
    TransformComponent transform;
    transform.position = spawnPosition != nullptr ? *spawnPosition : GetSpawnPositionInFrontOfCamera(8.0f);
    compMgr->AddComponent(entity, transform);

    LightComponent light;
    light.direction = glm::normalize(glm::vec3(-0.35f, -1.0f, -0.2f));
    light.color = glm::vec3(1.0f, 0.98f, 0.92f);
    light.intensity = 1.25f;
    compMgr->AddComponent(entity, light);

    meta.SetName(entity, "Directional Light");
    if (selectEntity)
    {
        selectedEntity = entity;
        renamingEntity = entity;
    }

    return entity;
}

void Editor::EnsureDefaultDirectionalLight()
{
    for (uint32_t id = 0; id < entityMgr->GetMaxEntities(); ++id)
    {
        const Entity entity{ id };
        if (entityMgr->IsAlive(entity) && compMgr->HasComponent<LightComponent>(entity))
        {
            if (!compMgr->HasComponent<TransformComponent>(entity))
            {
                TransformComponent transform;
                transform.position = GetSpawnPositionInFrontOfCamera(8.0f);
                compMgr->AddComponent(entity, transform);
            }
            return;
        }
    }

    CreateDirectionalLightEntity(nullptr, false);
}

void Editor::ApplyAssetToSelectedEntity(const AssetInfo& asset)
{
    if (!selectedEntity || !entityMgr->IsAlive(selectedEntity))
    {
        return;
    }

    if (asset.type == AssetType::Model)
    {
        if (!compMgr->HasComponent<TransformComponent>(selectedEntity))
        {
            compMgr->AddComponent(selectedEntity, TransformComponent{});
        }

        if (!compMgr->HasComponent<MeshComponent>(selectedEntity))
        {
            compMgr->AddComponent(selectedEntity, MeshComponent{});
        }

        auto& mesh = compMgr->GetComponent<MeshComponent>(selectedEntity);
        mesh.meshPath = asset.path;

        if (!compMgr->HasComponent<MaterialComponent>(selectedEntity))
        {
            compMgr->AddComponent(selectedEntity, MaterialComponent{});
        }
    }
    else if (asset.type == AssetType::Texture && compMgr->HasComponent<MaterialComponent>(selectedEntity))
    {
        auto& material = compMgr->GetComponent<MaterialComponent>(selectedEntity);
        material.albedoTexture = asset.path;
        material.useTexture = true;
    }
    else if (asset.type == AssetType::Script)
    {
        if (!compMgr->HasComponent<ScriptComponent>(selectedEntity))
        {
            compMgr->AddComponent(selectedEntity, ScriptComponent{});
        }

        auto& script = compMgr->GetComponent<ScriptComponent>(selectedEntity);
        script.ScriptPath = asset.path;
    }
}

// Editor Drawing
void Editor::Draw()
{
	

    if(!ProjectManager::HasActiveProject())
		return;

    SanitizeSelection();


    const auto& project = ProjectManager::GetActive();
    if (defaultLightProjectFile != project.projectFile)
    {
        defaultLightProjectFile = project.projectFile;
        EnsureDefaultDirectionalLight();
    }
	AppState appState = AppState::Editor;
    
    BeginDockSpace();

    ImGui::TextColored(
        engineMode == EngineMode::Play
        ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
        : ImVec4(1.0f, 1.0f, 0.2f, 1.0f),
        engineMode == EngineMode::Play ? "MODE: PLAY" : "MODE: EDITOR"
    );


    ImGuiIO& io = ImGui::GetIO();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
    {
        SceneSerializer::Save(project.scenePath.string(), *entityMgr, *compMgr, meta);
        statusText = "Scene Saved!";
        statusTimer = 2.0f; // show for 2 seconds
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
    {
        SceneSerializer::Load(project.scenePath.string(), *entityMgr, *compMgr, meta);
        EnsureDefaultDirectionalLight();
        statusText = "Scene Loaded!";
        statusTimer = 2.0f;
    }

// Top Bar
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene"))
            {
                entityMgr->Clear();
                compMgr->Clear();
                meta.Clear();
                selectedEntity = {};
                EnsureDefaultDirectionalLight();
            }

            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                SceneSerializer::Save(project.scenePath.string(), *entityMgr, *compMgr, meta);

            if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
            {
                SceneSerializer::Load(project.scenePath.string(), *entityMgr, *compMgr, meta);
                EnsureDefaultDirectionalLight();
            }

            if (selectedEntity && entityMgr->IsAlive(selectedEntity) && ImGui::MenuItem("Save Selected As Prefab"))
            {
                SaveSelectedAsPrefab();
            }

            if (ImGui::MenuItem("Set Current Scene As Startup"))
            {
                ProjectManager::SetStartupScene(project.scenePath);
            }

            if (ImGui::MenuItem("Build Prototype"))
            {
                PackageProject();
            }

            if (ImGui::MenuItem("Close Project"))
            {
                appState = AppState::Startup;
            }

            if (ImGui::MenuItem("Exit"))
                exit(0);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Add Entity"))
            {
                Entity e = entityMgr->CreateEntity();
                TransformComponent t;
                MeshComponent mesh;
                compMgr->AddComponent(e, t);
                compMgr->AddComponent(e, mesh);
                meta.SetName(e, "Entity " + std::to_string(e.id));
                selectedEntity = e;
                renamingEntity = e;
            }

            if (ImGui::MenuItem("Add Directional Light"))
            {
                CreateDirectionalLightEntity();
            }

            if (selectedEntity && entityMgr->IsAlive(selectedEntity))
            {
                if (ImGui::MenuItem("Duplicate Selected"))
                {
                    Entity clone = entityMgr->CreateEntity();
                    CloneKnownComponents(*compMgr, selectedEntity, clone);
                    meta.SetName(clone, meta.GetName(selectedEntity) + " (1)");
                }

                if (ImGui::MenuItem("Delete Selected"))
                {
                    RemoveKnownComponents(*compMgr, selectedEntity);
                    entityMgr->DestroyEntity(selectedEntity);
                    meta.Remove(selectedEntity);
                    selectedEntity = {};
                }
            }

            
            

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scripts"))
        {
            if (ImGui::MenuItem("New Script"))
            {
                requestCreateScriptPopup = true;
            }

            if (activeScriptIndex >= 0)
            {
                if (ImGui::MenuItem("Save Active Script", "Ctrl+S"))
                {
                    SaveScript(openScripts[activeScriptIndex]);
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Lua Documentation"))
            {
                ShellExecuteA(
                    nullptr,
                    "open",
                    "https://www.lua.org/manual/5.4/",
                    nullptr,
                    nullptr,
                    SW_SHOWNORMAL
                );
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug"))
        {
            ImGui::MenuItem(
                "Input Debug Overlay",
                nullptr,
                &showInputDebug
            );
            if (renderer != nullptr)
            {
                ImGui::MenuItem(
                    "Collision Debug Overlay",
                    nullptr,
                    &renderer->collisionDebugVisible
                );
                ImGui::MenuItem(
                    "Grounded Indicator",
                    nullptr,
                    &renderer->collisionGroundedVisible
                );
                ImGui::MenuItem(
                    "Velocity Indicator",
                    nullptr,
                    &renderer->collisionVelocityVisible
                );
                ImGui::MenuItem(
                    "Light Gizmos",
                    nullptr,
                    &renderer->lightGizmoVisible
                );
            }

            ImGui::EndMenu();
        }

        if(ImGui::Button("Play"))
        {
            TogglePlayMode();
		}

        ImGui::EndMainMenuBar();
    }



    if (requestCreateScriptPopup)
    {
        ImGui::OpenPopup("Create Lua Script");
        requestCreateScriptPopup = false;
    }

    if (!openScripts.empty())
    {
		DrawScriptDocsPanel();
    }

    DrawHierarchy();
    DrawSceneView();
    DrawDetails();
    DrawProjectSettingsPanel();
	DrawAssetsPanel();
    DrawScriptEditor();
    DrawCreateScriptPopup();
	DrawConsoleWindow();
    DrawInputDebug(*inputSystem);
}

void Editor::SanitizeSelection()
{
    if (selectedEntity && !entityMgr->IsAlive(selectedEntity))
    {
        selectedEntity = {};
    }

    if (renamingEntity && !entityMgr->IsAlive(renamingEntity))
    {
        renamingEntity = {};
    }
}

// scene view panel (center)
void Editor::DrawSceneView()
{
    
    ImGuiIO& io = ImGui::GetIO();

    ImGui::Begin("Scene");

    // main rendering
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int texW = (int)avail.x;
    int texH = (int)avail.y;

    if (renderer && camera && entityMgr && compMgr)
    {
        renderer->editorMode = true;
        renderer->RenderToTexture(*entityMgr, *compMgr, *camera, texW, texH, selectedEntity);
        static const char* renderModeNames[] = { "Lit", "Unlit", "Reflections", "Wireframe", "Normals" };
        int renderModeIndex = static_cast<int>(renderer->viewMode);
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::Combo("##SceneRenderMode", &renderModeIndex, renderModeNames, IM_ARRAYSIZE(renderModeNames)))
        {
            renderer->viewMode = static_cast<Renderer::ViewMode>(renderModeIndex);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Post FX", &renderer->postProcessingEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("Exposure", &renderer->exposure, 0.01f, 0.1f, 4.0f);
        ImTextureID tex = (ImTextureID)(intptr_t)renderer->GetSceneTextureID();

        ImVec2 sceneImageMin = ImGui::GetCursorScreenPos();
        ImVec2 sceneImageMax(sceneImageMin.x + avail.x, sceneImageMin.y + avail.y);
        if (tex)
        {
            ImGui::Image(tex, avail, ImVec2(0, 1), ImVec2(1, 0));
            sceneImageMin = ImGui::GetItemRectMin();
            sceneImageMax = ImGui::GetItemRectMax();
        }

        if (ImGui::BeginDragDropTarget())
        {
            std::string assetPath;
            Vec3 dropPosition = GetSpawnPositionInFrontOfCamera();
            TryGetSceneDropPosition(sceneImageMin, sceneImageMax, dropPosition);

            if (AcceptAssetPayload(kAssetPayloadModel, assetPath))
            {
                if (const AssetInfo* asset = FindAssetByPath(assetPath))
                {
                    CreateEntityFromAsset(*asset, &dropPosition);
                }
            }
            else if (AcceptAssetPayload(kAssetPayloadScript, assetPath))
            {
                if (const AssetInfo* asset = FindAssetByPath(assetPath))
                {
                    CreateEntityFromAsset(*asset, &dropPosition);
                }
            }
            else if (AcceptAssetPayload(kAssetPayloadPrefab, assetPath))
            {
                if (const AssetInfo* asset = FindAssetByPath(assetPath))
                {
                    CreateEntityFromAsset(*asset, &dropPosition);
                }
            }

            ImGui::EndDragDropTarget();
        }
    }
    else
    {
        ImGui::TextDisabled("Renderer, Camera, or Streamer not assigned.");
        ImGui::End();
        return;
    }

	//Click selection
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
    ImVec2 contentMax = ImGui::GetWindowContentRegionMax();

    ImVec2 sceneOrigin = { windowPos.x + contentMin.x, windowPos.y + contentMin.y };
    ImVec2 sceneSize = { contentMax.x - contentMin.x, contentMax.y - contentMin.y };

    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 localMouse = { mousePos.x - sceneOrigin.x, mousePos.y - sceneOrigin.y };

        if (localMouse.x >= 0 && localMouse.y >= 0 &&
            localMouse.x < sceneSize.x && localMouse.y < sceneSize.y)
        {
            glm::vec2 ndc;
            ndc.x = (2.0f * localMouse.x) / sceneSize.x - 1.0f;
            ndc.y = 1.0f - (2.0f * localMouse.y) / sceneSize.y;

            glm::mat4 proj = glm::perspective(glm::radians(camera->fov), camera->aspect, camera->nearPlane, camera->farPlane);
            glm::mat4 view = glm::lookAt(camera->position, camera->position + camera->forward, camera->up);
            glm::mat4 invVP = glm::inverse(proj * view);

            glm::vec4 nearWorld = invVP * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
            glm::vec4 farWorld = invVP * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
            nearWorld /= nearWorld.w;
            farWorld /= farWorld.w;

            glm::vec3 rayOrigin = glm::vec3(nearWorld);
            glm::vec3 rayDir = glm::normalize(glm::vec3(farWorld - nearWorld));

            float closestT = FLT_MAX;
            Entity closestEntity;

            for (uint32_t id = 0; id < entityMgr->GetMaxEntities(); ++id)
            {
                Entity e{ id };
                if (!entityMgr->IsAlive(e) ||
                    !compMgr->HasComponent<TransformComponent>(e) ||
                    !compMgr->HasComponent<MeshComponent>(e))
                {
                    continue;
                }

                const auto& t = compMgr->GetComponent<TransformComponent>(e);
                const glm::vec3 pos(t.position.x, t.position.y, t.position.z);
                const glm::vec3 half(0.5f * t.scale.x, 0.5f * t.scale.y, 0.5f * t.scale.z);

                const glm::vec3 aabbMin = pos - half;
                const glm::vec3 aabbMax = pos + half;

                float tHit = 0.0f;
                if (RayIntersectsAABB(rayOrigin, rayDir, aabbMin, aabbMax, tHit) &&
                    tHit < closestT &&
                    tHit > 0.0f)
                {
                    closestT = tHit;
                    closestEntity = e;
                }
            }

            //  Select or deselect
            if (closestEntity) selectedEntity = closestEntity;
            else selectedEntity = {};
        }
    }

    //ImGuizmo Manipulation
    if (selectedEntity &&
        entityMgr->IsAlive(selectedEntity) &&
        compMgr->HasComponent<TransformComponent>(selectedEntity))
    {
        auto& transform = compMgr->GetComponent<TransformComponent>(selectedEntity);

        ImGuizmo::BeginFrame();
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();

        // Restrict gizmo to Scene window content area
        ImGuizmo::SetRect(sceneOrigin.x, sceneOrigin.y, sceneSize.x, sceneSize.y);

        glm::mat4 view = glm::lookAt(camera->position, camera->position + camera->forward, camera->up);
        glm::mat4 proj = glm::perspective(glm::radians(camera->fov), camera->aspect, camera->nearPlane, camera->farPlane);

        glm::mat4 model(1.0f);
        model = glm::translate(model, glm::vec3(transform.position.x, transform.position.y, transform.position.z));
        model = glm::rotate(model, glm::radians(transform.rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(transform.rotation.z), glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(transform.scale.x, transform.scale.y, transform.scale.z));

        // ---------------- Gizmo controls ----------------
        EditorGizmoController::TickHotkeys(gizmoState);
		renderer->snapGridVisible = gizmoState.snapEnabled;
        renderer->snapStep = gizmoState.snapValues[0];

        // Disable camera control while dragging
        if (ImGuizmo::IsUsing())
            SDL_SetRelativeMouseMode(SDL_FALSE);
        else if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsOver())
            SDL_SetRelativeMouseMode(SDL_FALSE);

    

        // Manipulate
        if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0],
            gizmoState.operation, gizmoState.mode,
            &model[0][0],
            nullptr,
            gizmoState.snapEnabled ? gizmoState.snapValues : nullptr))
        {
            glm::vec3 trans, rot, scl;
            ImGuizmo::DecomposeMatrixToComponents(&model[0][0],
                &trans.x, &rot.x, &scl.x);
            transform.position = { trans.x, trans.y, trans.z };
            transform.rotation = { rot.x, rot.y, rot.z };
            transform.scale = { scl.x, scl.y, scl.z };
        }

        // Optional HUD
        ImVec2 hudPos = { sceneOrigin.x + 10.0f, sceneOrigin.y + 10.0f };
        ImGui::SetCursorScreenPos(hudPos);
        ImGui::Text("Gizmo: %s | Space: %s | Snap: %s",
            gizmoState.operation == ImGuizmo::TRANSLATE ? "Translate" :
            gizmoState.operation == ImGuizmo::ROTATE ? "Rotate" : "Scale",
            gizmoState.mode == ImGuizmo::WORLD ? "World" : "Local",
            gizmoState.snapEnabled ? "ON (Ctrl)" : "OFF");
    }
    else if (selectedEntity && entityMgr->IsAlive(selectedEntity))
    {
        ImVec2 hudPos = { sceneOrigin.x + 10.0f, sceneOrigin.y + 10.0f };
        ImGui::SetCursorScreenPos(hudPos);
        ImGui::TextDisabled("Selected entity has no TransformComponent.");
    }

    if (statusTimer > 0.0f)
    {
        statusTimer -= ImGui::GetIO().DeltaTime;
        //float alpha = std::min(statusTimer / 2.0f, 1.0f); // fade out near end

        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + 10, ImGui::GetWindowPos().y + 10));
       // ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, alpha));
        ImVec2 center = ImVec2(ImGui::GetWindowPos().x + ImGui::GetContentRegionAvail().x * 0.5f,
            ImGui::GetWindowPos().y + 40.0f);
        ImGui::SetCursorScreenPos(center);
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text("%s", statusText.c_str());
    }

    ImGui::End();
}


//Hierarchy Panel (left)
void Editor::DrawHierarchy()
{
    ImGuiIO& io = ImGui::GetIO();

    

    ImGui::Begin("Hierarchy");

    if (!entityMgr || !compMgr) {
        ImGui::TextDisabled("Error: missing ECS references!");
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Entities");
    ImGui::Separator();

    if (ImGui::Button("+ Add Entity", ImVec2(-1, 0)))
    {
        Entity e = entityMgr->CreateEntity();

        TransformComponent t;
        MeshComponent mesh;
        t.position = { 0, 0, 0 };
        t.rotation = { 0, 0, 0 };
        t.scale = { 1, 1, 1 };
        compMgr->AddComponent(e, t);
        compMgr->AddComponent(e, mesh);

        std::string name = "Entity " + std::to_string(e.id);
        meta.SetName(e, name);

        selectedEntity = e;
        renamingEntity = e; // immediately enter rename mode
    }

    ImGui::Separator();

    for (uint32_t id = 0; id < entityMgr->GetMaxEntities(); ++id)
    {
        Entity e{ id };
        if (!entityMgr->IsAlive(e)) continue;

        std::string& name = meta.names.count(e.id)
            ? meta.names[e.id]
            : meta.names[e.id] = "Entity " + std::to_string(e.id);

        bool selected = (e.id == selectedEntity.id);
        bool openForRename = (renamingEntity.id == e.id);

        // Inline rename mode
        if (openForRename)
        {
            ImGui::PushID(e.id);
            ImGui::SetNextItemWidth(-1);  // Make it fill full width
            ImGui::SetKeyboardFocusHere(-1); // Autofocus on open

            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.25f, 1.0f)); // Slight contrast
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

            static char buffer[128];
            strncpy_s(buffer, name.c_str(), sizeof(buffer));
            buffer[sizeof(buffer) - 1] = '\0';

            // Create a dummy line so InputText isn't underneath the selectable highlight
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 0.2f));

          
            if (ImGui::InputText("##edit", buffer, IM_ARRAYSIZE(buffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                meta.SetName(e, buffer);
                renamingEntity = {}; // exit rename mode
            }

            // Cancel rename if focus lost
            if (!ImGui::IsItemActive() && !ImGui::IsItemFocused())
                renamingEntity = {};

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        else
        {
            if (ImGui::Selectable(name.c_str(), selected))
                selectedEntity = e;
        }

        
        if (ImGui::BeginPopupContextItem(std::to_string(e.id).c_str()))
        {
            if (ImGui::MenuItem("Rename"))
                renamingEntity = e;

            if (ImGui::MenuItem("Duplicate"))
            {
                Entity clone = entityMgr->CreateEntity();
                CloneKnownComponents(*compMgr, e, clone);
                std::string newName = name + " (1)";
                meta.SetName(clone, newName);
            }

            if (ImGui::MenuItem("Delete"))
            {
                RemoveKnownComponents(*compMgr, e);
                entityMgr->DestroyEntity(e);
                meta.Remove(e);
                if (selectedEntity.id == e.id) selectedEntity = {};
            }

            ImGui::EndPopup();
        }
    }


    ImGui::End();
}

//Details Area
void Editor::DrawDetails()
{
    ImGui::Begin("Details");

    // ---------------- Safety gates ----------------
    if (!entityMgr)
    {
        ImGui::TextDisabled("Error: entityMgr is null!");
        ImGui::End();
        return;
    }

    if (!entityMgr->IsAlive(selectedEntity))
    {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    // From here on, selectedEntity is GUARANTEED valid
    ImGui::Text("Entity ID: %u", selectedEntity.id);

    // ---------------- Transform ----------------
    if (compMgr->HasComponent<TransformComponent>(selectedEntity))
    {
        auto& transform =
            compMgr->GetComponent<TransformComponent>(selectedEntity);

        ImGui::SeparatorText("Transform");
        ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
        ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.1f);
        ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f, 0.01f, 10.0f);
    }

    bool hasMesh =
        compMgr->HasComponent<MeshComponent>(selectedEntity);

    if (ImGui::Checkbox("Renderable Mesh", &hasMesh))
    {
        if (hasMesh)
        {
            compMgr->AddComponent(selectedEntity, MeshComponent{});
        }
        else
        {
            RemoveComponentIfPresent<MeshComponent>(*compMgr, selectedEntity);
            RemoveComponentIfPresent<MaterialComponent>(*compMgr, selectedEntity);
        }
    }

    if (hasMesh && compMgr->HasComponent<MeshComponent>(selectedEntity))
    {
        auto& mesh = compMgr->GetComponent<MeshComponent>(selectedEntity);
        char meshPathBuffer[260] = {};
        strncpy_s(meshPathBuffer, mesh.meshPath.c_str(), sizeof(meshPathBuffer) - 1);

        ImGui::SeparatorText("Mesh");
        ImGui::TextWrapped("Mesh Path: %s", mesh.meshPath.c_str());
        if (ImGui::InputText("Mesh Path", meshPathBuffer, IM_ARRAYSIZE(meshPathBuffer)))
        {
            mesh.meshPath = meshPathBuffer;
            if (mesh.meshPath.empty())
            {
                mesh.meshPath = "builtin://cube";
            }
        }
        ImGui::TextDisabled("Drop a model asset here to assign it.");
        if (ImGui::BeginDragDropTarget())
        {
            std::string assetPath;
            if (AcceptAssetPayload(kAssetPayloadModel, assetPath))
            {
                mesh.meshPath = assetPath;
            }
            ImGui::EndDragDropTarget();
        }
        if (selectedAsset != nullptr && selectedAsset->type == AssetType::Model)
        {
            if (ImGui::Button("Use Selected Model"))
            {
                mesh.meshPath = selectedAsset->path;
            }
        }
    }

    bool hasMaterial =
        compMgr->HasComponent<MaterialComponent>(selectedEntity);

    if (hasMesh)
    {
        if (!hasMaterial)
        {
            if (ImGui::Button("Add Material"))
            {
                compMgr->AddComponent(selectedEntity, MaterialComponent{});
                hasMaterial = true;
            }
        }
        else
        {
            auto& material =
                compMgr->GetComponent<MaterialComponent>(selectedEntity);

            ImGui::SeparatorText("Material");
            ImGui::ColorEdit3("Albedo", &material.albedo.x);
            ImGui::SliderFloat("Specular", &material.specular, 0.0f, 1.0f);
            ImGui::SliderFloat("Shininess", &material.shininess, 1.0f, 128.0f);
            ImGui::Checkbox("Use Texture", &material.useTexture);
            char texturePathBuffer[260] = {};
            strncpy_s(texturePathBuffer, material.albedoTexture.c_str(), sizeof(texturePathBuffer) - 1);
            if (ImGui::InputText("Albedo Texture", texturePathBuffer, IM_ARRAYSIZE(texturePathBuffer)))
            {
                material.albedoTexture = texturePathBuffer;
            }
            ImGui::TextDisabled("Drop a texture asset here to assign it.");
            if (ImGui::BeginDragDropTarget())
            {
                std::string assetPath;
                if (AcceptAssetPayload(kAssetPayloadTexture, assetPath))
                {
                    material.albedoTexture = assetPath;
                    material.useTexture = true;
                }
                ImGui::EndDragDropTarget();
            }
            if (selectedAsset != nullptr && selectedAsset->type == AssetType::Texture)
            {
                if (ImGui::Button("Use Selected Texture"))
                {
                    material.albedoTexture = selectedAsset->path;
                    material.useTexture = true;
                }
            }

            if (ImGui::Button("Remove Material"))
            {
                compMgr->RemoveComponent<MaterialComponent>(selectedEntity);
                hasMaterial = false;
            }
        }
    }

    bool hasLight =
        compMgr->HasComponent<LightComponent>(selectedEntity);

    if (ImGui::Checkbox("Directional Light", &hasLight))
    {
        if (hasLight)
        {
            if (!compMgr->HasComponent<TransformComponent>(selectedEntity))
            {
                compMgr->AddComponent(selectedEntity, TransformComponent{});
            }
            compMgr->AddComponent(selectedEntity, LightComponent{});
        }
        else
        {
            RemoveComponentIfPresent<LightComponent>(*compMgr, selectedEntity);
        }
    }

    if (hasLight && compMgr->HasComponent<LightComponent>(selectedEntity))
    {
        auto& light =
            compMgr->GetComponent<LightComponent>(selectedEntity);

        if (!compMgr->HasComponent<TransformComponent>(selectedEntity))
        {
            if (ImGui::Button("Add Transform For Light"))
            {
                TransformComponent transform;
                transform.position = GetSpawnPositionInFrontOfCamera(8.0f);
                compMgr->AddComponent(selectedEntity, transform);
            }
            ImGui::TextDisabled("Light direction is used for lighting. Add a transform to place the gizmo/origin in world space.");
        }

        ImGui::SeparatorText("Light");
        ImGui::TextDisabled("Transform = world origin, Direction = emitted light direction.");
        ImGui::DragFloat3("Direction", &light.direction.x, 0.05f, -1.0f, 1.0f);
        if (glm::length(light.direction) <= 0.0001f)
        {
            light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        }
        ImGui::ColorEdit3("Color", &light.color.x);
        ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 10.0f);
        ImGui::Checkbox("Light Enabled", &light.enabled);
        ImGui::TextDisabled("Directional lights act as the current sun/GI driver and update in realtime.");
    }

    // ---------------- Asset info ----------------
    if (selectedAsset)
    {
        ImGui::SeparatorText("Asset Info");
        ImGui::Text("Name: %s", selectedAsset->name.c_str());
        ImGui::Text("Path: %s", selectedAsset->path.c_str());

        if (selectedAsset->type == AssetType::Texture)
        {
            ImGui::Text(
                "Resolution: %d x %d",
                selectedAsset->width,
                selectedAsset->height
            );
            ImGui::Text(
                "Channels: %d",
                selectedAsset->channels
            );
        }
    }

    // ---------------- Physics ----------------
    bool hasPhysics =
        compMgr->HasComponent<PhysicsComponent>(selectedEntity);

    if (ImGui::Checkbox("Enable Physics", &hasPhysics))
    {
        if (hasPhysics &&
            !compMgr->HasComponent<PhysicsComponent>(selectedEntity))
        {
            compMgr->AddComponent(
                selectedEntity,
                PhysicsComponent{}
            );
        }
        else if (!hasPhysics &&
            compMgr->HasComponent<PhysicsComponent>(selectedEntity))
        {
            compMgr->RemoveComponent<PhysicsComponent>(selectedEntity);
        }
    }

    ImGui::Separator();

    bool hasCollider =
        compMgr->HasComponent<ColliderComponent>(selectedEntity);

    if (ImGui::Checkbox("Enable Collision", &hasCollider))
    {
        if (hasCollider &&
            !compMgr->HasComponent<ColliderComponent>(selectedEntity))
        {
            compMgr->AddComponent(
                selectedEntity,
                ColliderComponent{}
            );
        }
        else if (!hasCollider &&
            compMgr->HasComponent<ColliderComponent>(selectedEntity))
        {
            compMgr->RemoveComponent<ColliderComponent>(selectedEntity);
        }
    }

    if (hasCollider &&
        compMgr->HasComponent<ColliderComponent>(selectedEntity))
    {
        auto& collider =
            compMgr->GetComponent<ColliderComponent>(selectedEntity);

        ImGui::Indent();

        // --------------------------------------------------------
        // PRESET DROPDOWN
        // --------------------------------------------------------
        static int selectedPreset = 0;

        if (ImGui::BeginCombo(
            "Collider Preset",
            g_ColliderPresets[selectedPreset].name))
        {
            for (int i = 0; i < kColliderPresetCount; ++i)
            {
                bool isSelected = (selectedPreset == i);
                if (ImGui::Selectable(g_ColliderPresets[i].name, isSelected))
                {
                    selectedPreset = i;

                    // APPLY PRESET IMMEDIATELY
                    collider.halfExtents =
                        g_ColliderPresets[i].halfExtents;

                    collider.isStatic =
                        g_ColliderPresets[i].isStatic;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // --------------------------------------------------------
        // MANUAL OVERRIDES
        // --------------------------------------------------------
        ImGui::DragFloat3(
            "Half Extents",
            &collider.halfExtents.x,
            0.05f,
            0.01f,
            1000.0f
        );

        ImGui::Checkbox("Static", &collider.isStatic);

        ImGui::Unindent();
    }

	ImGui::Separator();

    // ---------------- Script ----------------
    if (compMgr->HasComponent<ScriptComponent>(selectedEntity))
    {
        auto& sc =
            compMgr->GetComponent<ScriptComponent>(selectedEntity);

        ImGui::SeparatorText("Script");

        auto scripts = GetProjectLuaScripts();
        scripts.insert(scripts.begin(), "<New Script>");

        int currentIndex = 0;
        for (int i = 1; i < (int)scripts.size(); ++i)
        {
            if (scripts[i] == sc.ScriptPath)
            {
                currentIndex = i;
                break;
            }
        }

        if (ImGui::BeginCombo(
            "Script File",
            scripts[currentIndex].c_str()))
        {
            for (int i = 0; i < (int)scripts.size(); ++i)
            {
                bool selected = (i == currentIndex);
                if (ImGui::Selectable(scripts[i].c_str(), selected))
                {
                    if (scripts[i] == "<New Script>")
                    {
                        requestCreateScriptPopup = true;
                    }
                    else
                    {
                        sc.ScriptPath = scripts[i];

                        const auto& project =
                            ProjectManager::GetActive();

                        OpenScriptFile(
                            (project.rootPath / sc.ScriptPath).string()
                        );
                        FocusScriptEditor();
                    }
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TextDisabled("Drop a script asset here to assign it.");
        if (ImGui::BeginDragDropTarget())
        {
            std::string assetPath;
            if (AcceptAssetPayload(kAssetPayloadScript, assetPath))
            {
                sc.ScriptPath = assetPath;
            }
            ImGui::EndDragDropTarget();
        }

        if (!sc.ScriptPath.empty())
        {
            if (ImGui::Button("Open Script"))
            {
                const auto& project =
                    ProjectManager::GetActive();

                OpenScriptFile(
                    (project.rootPath / sc.ScriptPath).string()
                );
                FocusScriptEditor();
            }

            ImGui::SameLine();

            if (ImGui::Button("Reload Script"))
            {
                scriptSystem->LoadScript(sc);
            }
        }
    }
    else
    {
        if (ImGui::Button("Add Script"))
        {
            ScriptComponent sc;
            sc.ScriptPath = "";
            compMgr->AddComponent(selectedEntity, sc);
        }
    }

    ImGui::Separator();

    bool hasAudioSource =
        compMgr->HasComponent<AudioSourceComponent>(selectedEntity);

    if (ImGui::Checkbox("Audio Source", &hasAudioSource))
    {
        if (hasAudioSource)
        {
            compMgr->AddComponent(selectedEntity, AudioSourceComponent{});
        }
        else
        {
            RemoveComponentIfPresent<AudioSourceComponent>(*compMgr, selectedEntity);
        }
    }

    if (hasAudioSource && compMgr->HasComponent<AudioSourceComponent>(selectedEntity))
    {
        auto& audio = compMgr->GetComponent<AudioSourceComponent>(selectedEntity);
        char audioPathBuffer[260] = {};
        strncpy_s(audioPathBuffer, audio.path.c_str(), sizeof(audioPathBuffer) - 1);

        ImGui::SeparatorText("Audio Source");
        if (ImGui::InputText("Audio Path", audioPathBuffer, IM_ARRAYSIZE(audioPathBuffer)))
        {
            audio.path = audioPathBuffer;
        }
        if (ImGui::BeginDragDropTarget())
        {
            std::string assetPath;
            if (AcceptAssetPayload(kAssetPayloadAudio, assetPath))
            {
                audio.path = assetPath;
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::Checkbox("Loop", &audio.loop);
        ImGui::Checkbox("Play On Start", &audio.playOnStart);
        ImGui::SliderFloat("Volume", &audio.volume, 0.0f, 1.0f);
        ImGui::Checkbox("Enabled", &audio.enabled);
        ImGui::TextDisabled("Spatial playback is reserved for a later prototype pass.");
    }

    bool hasAnimation = compMgr->HasComponent<AnimationComponent>(selectedEntity);
    if (ImGui::Checkbox("Transform Animation", &hasAnimation))
    {
        if (hasAnimation)
        {
            compMgr->AddComponent(selectedEntity, AnimationComponent{});
        }
        else
        {
            RemoveComponentIfPresent<AnimationComponent>(*compMgr, selectedEntity);
        }
    }

    if (hasAnimation && compMgr->HasComponent<AnimationComponent>(selectedEntity))
    {
        auto& animation = compMgr->GetComponent<AnimationComponent>(selectedEntity);
        ImGui::SeparatorText("Animation");
        ImGui::Checkbox("Playing", &animation.playing);
        ImGui::Checkbox("Loop", &animation.loop);
        ImGui::DragFloat("Clip Duration", &animation.clip.duration, 0.01f, 0.0f, 60.0f);
        ImGui::DragFloat("Current Time", &animation.currentTime, 0.01f, 0.0f, std::max(animation.clip.duration, 0.0f));
    }

    bool hasNavAgent = compMgr->HasComponent<NavAgentComponent>(selectedEntity);
    if (ImGui::Checkbox("Navigation Agent", &hasNavAgent))
    {
        if (hasNavAgent)
        {
            compMgr->AddComponent(selectedEntity, NavAgentComponent{});
        }
        else
        {
            RemoveComponentIfPresent<NavAgentComponent>(*compMgr, selectedEntity);
        }
    }

    if (hasNavAgent && compMgr->HasComponent<NavAgentComponent>(selectedEntity))
    {
        auto& nav = compMgr->GetComponent<NavAgentComponent>(selectedEntity);
        static const char* navModeNames[] = { "Patrol", "Follow", "Chase" };
        int navMode = static_cast<int>(nav.mode);
        ImGui::SeparatorText("Navigation");
        if (ImGui::Combo("Agent Mode", &navMode, navModeNames, IM_ARRAYSIZE(navModeNames)))
        {
            nav.mode = static_cast<NavAgentMode>(navMode);
        }
        ImGui::DragFloat("Move Speed", &nav.speed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("Stopping Distance", &nav.stoppingDistance, 0.01f, 0.0f, 5.0f);
        ImGui::Checkbox("Loop Waypoints", &nav.loop);
        ImGui::Checkbox("Active Agent", &nav.active);
    }

    bool hasRuntimeUI = compMgr->HasComponent<RuntimeUIComponent>(selectedEntity);
    if (ImGui::Checkbox("Runtime UI", &hasRuntimeUI))
    {
        if (hasRuntimeUI)
        {
            compMgr->AddComponent(selectedEntity, RuntimeUIComponent{});
        }
        else
        {
            RemoveComponentIfPresent<RuntimeUIComponent>(*compMgr, selectedEntity);
        }
    }

    if (hasRuntimeUI && compMgr->HasComponent<RuntimeUIComponent>(selectedEntity))
    {
        auto& ui = compMgr->GetComponent<RuntimeUIComponent>(selectedEntity);
        static const char* uiTypeNames[] = { "Text", "Button", "Image" };
        static const char* uiAnchorNames[] = { "Top Left", "Top Center", "Top Right", "Center", "Bottom Left", "Bottom Center", "Bottom Right" };
        int uiType = static_cast<int>(ui.type);
        int uiAnchor = static_cast<int>(ui.anchor);
        ImGui::SeparatorText("Runtime UI");
        if (ImGui::Combo("UI Type", &uiType, uiTypeNames, IM_ARRAYSIZE(uiTypeNames)))
        {
            ui.type = static_cast<RuntimeUIElementType>(uiType);
        }
        if (ImGui::Combo("Anchor", &uiAnchor, uiAnchorNames, IM_ARRAYSIZE(uiAnchorNames)))
        {
            ui.anchor = static_cast<RuntimeUIAnchor>(uiAnchor);
        }
        char uiTextBuffer[260] = {};
        strncpy_s(uiTextBuffer, ui.text.c_str(), sizeof(uiTextBuffer) - 1);
        if (ImGui::InputText("UI Text", uiTextBuffer, IM_ARRAYSIZE(uiTextBuffer)))
        {
            ui.text = uiTextBuffer;
        }
        ImGui::DragFloat2("UI Offset", &ui.offsetX, 1.0f);
        ImGui::DragFloat2("UI Size", &ui.width, 1.0f, 1.0f, 1024.0f);
        ImGui::Checkbox("Visible", &ui.visible);
    }

    if (compMgr->HasComponent<PlayerControllerComponent>(selectedEntity))
    {
        auto& pc =
            compMgr->GetComponent<PlayerControllerComponent>(selectedEntity);

        if (ImGui::CollapsingHeader("Player Controller", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat(
                "Move Speed",
                &pc.moveSpeed,
                0.1f, 0.0f, 50.0f
            );

            ImGui::DragFloat(
                "Look Speed",
                &pc.lookSpeed,
                0.01f, 0.0f, 5.0f
            );

            ImGui::Checkbox(
                "Allow Camera Switching",
                &pc.allowCameraSwitch
            );
        }
    }


    // ---------------- Camera Follow ----------------
    if (compMgr->HasComponent<CameraFollowComponent>(selectedEntity))
    {
        auto& c =
            compMgr->GetComponent<CameraFollowComponent>(selectedEntity);

        ImGui::Separator();

        if (ImGui::CollapsingHeader("Camera Follow"))
        {
            ImGui::DragFloat(
                "Distance",
                &c.distance,
                0.1f,
                0.0f,
                20.0f
            );
            ImGui::DragFloat(
                "Height",
                &c.height,
                0.1f,
                -5.0f,
                10.0f
            );
            ImGui::DragFloat(
                "Smoothness",
                &c.smoothness,
                0.1f,
                0.1f,
                50.0f
            );
        }
    }

    ImGui::End();
}





void Editor::DrawAssetsPanel()
{

    
    ImGui::Begin("Assets");

    ImGui::TextDisabled("Project Assets");
    ImGui::Separator();

    static bool showImportPopup = false;
    if (ImGui::Button("+ Import Asset", ImVec2(200, 30)))
        ImGui::OpenPopup("Import Asset");

   

    static char newScriptName[64] = "";

    /*if (ImGui::BeginPopupModal("Create Lua Script", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Script name:");
        ImGui::InputText("##ScriptName", newScriptName, sizeof(newScriptName));

        ImGui::Spacing();

        if (ImGui::Button("Create", ImVec2(120, 0)))
        {
            if (strlen(newScriptName) > 0)
            {
                CreateLuaScript(newScriptName);
                newScriptName[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            newScriptName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }*/

    if (ImGui::BeginPopupModal("Import Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Select type of asset to import:");
        ImGui::Separator();
        static int selectedType = -1;

        if (ImGui::Button("Model", ImVec2(150, 30))) selectedType = 0;
        if (ImGui::Button("Texture", ImVec2(150, 30))) selectedType = 1;
        if (ImGui::Button("Audio", ImVec2(150, 30))) selectedType = 2;

        if (selectedType != -1)
        {
            OPENFILENAMEA ofn;
            CHAR szFile[260] = { 0 };
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = nullptr;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            switch (selectedType)
            {
            case 0: // Model
                ofn.lpstrFilter =
                    "Model Files (*.obj;*.fbx;*.gltf;*.glb)\0*.obj;*.fbx;*.gltf;*.glb\0All Files\0*.*\0";
                break;
            case 1: // Texture
                ofn.lpstrFilter =
                    "Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
                break;
            case 2: // Audio
                ofn.lpstrFilter =
                    "Audio Files (*.wav;*.mp3;*.ogg)\0*.wav;*.mp3;*.ogg\0All Files\0*.*\0";
                break;
            }
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn))
            {
                std::string filePath = ofn.lpstrFile;
                std::replace(filePath.begin(), filePath.end(), '\\', '/');

                switch (selectedType)
                {
                case 0: AssetImporter::ImportModelAsset(filePath); break;
                case 1: AssetImporter::ImportTexture(filePath); break;
                case 2: AssetImporter::ImportAudio(filePath); break;
                }

                if (!AssetImporter::GetLastError().empty())
                {
                    EditorConsole::Error(AssetImporter::GetLastError());
                }
                else
                {
                    RefreshAssetDatabase();
                }
            }
            selectedType = -1;
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel", ImVec2(100, 25))) {
            selectedType = -1;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::Separator();

    static char searchBuf[128] = "";
    ImGui::InputTextWithHint("##SearchAssets", "Search assets...", searchBuf, IM_ARRAYSIZE(searchBuf));
    ImGui::Separator();

    std::string query = searchBuf;
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);


    // --- Grid of Assets ---
    if (!assetsScanned)
    {
        RefreshAssetDatabase();
    }

    auto& assets = assetDB.GetAssets();
    int itemsPerRow = 6;
    int itemCount = 0;

    for (auto& a : assets)
    {
        std::string nameLower = a.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (!query.empty() && nameLower.find(query) == std::string::npos)
        {
            continue;
        }

        ImGui::BeginGroup();

        if (a.type == AssetType::Texture)
        {
            GLuint tex = LoadTextureForPreview(a);
            if (tex)
                ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(64, 64));
            else
                ImGui::Button("IMG", ImVec2(64, 64));
        }
        else if (a.type == AssetType::Model)
            ImGui::Button("MDL", ImVec2(64, 64));
        else if (a.type == AssetType::Audio)
            ImGui::Button("AUD", ImVec2(64, 64));
        else if (a.type == AssetType::Script)
            ImGui::Button("LUA", ImVec2(64, 64));
        else if (a.type == AssetType::Prefab)
            ImGui::Button("PFB", ImVec2(64, 64));
        else if (a.type == AssetType::Scene)
            ImGui::Button("SCN", ImVec2(64, 64));
        else
            ImGui::Button("?", ImVec2(64, 64));

        if (ImGui::IsItemClicked())
            selectedAsset = &a;

        if (const char* payloadType = GetPayloadType(a.type); payloadType != nullptr &&
            ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload(payloadType, a.path.c_str(), a.path.size() + 1);
            ImGui::TextUnformatted(a.name.c_str());
            ImGui::TextDisabled("%s", assetDB.AssetTypeToString(a.type));
            ImGui::EndDragDropSource();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (a.type == AssetType::Model || a.type == AssetType::Script || a.type == AssetType::Prefab)
            {
                CreateEntityFromAsset(a);
            }
        }

        ImGui::TextWrapped("%s", a.name.c_str());
        ImGui::EndGroup();

        if (++itemCount % itemsPerRow != 0)
            ImGui::SameLine();
    }

    ImGui::End();

}

GLuint Editor::LoadTextureForPreview(AssetInfo& a)
{
    if (a.previewLoaded && a.previewID != 0)
    {
        return a.previewID;
    }

    const GLuint tex = ResourceManager::GetTexture(a.path);
    if (tex == 0)
    {
        std::cerr << "[Thumbnail] Failed to load preview: " << a.path << "\n";
        return 0;
    }

    a.previewID = tex;
    a.previewLoaded = true;
    return tex;
}



void Editor::BeginDockSpace()
{
    static bool dockspaceInitialized = false;

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("DockSpaceRoot", nullptr, windowFlags);

    ImGui::PopStyleVar(2);

    ImGuiID dockspaceID = ImGui::GetID("MainDockSpace");

    // Always create the dockspace every frame
    ImGui::DockSpace(
        dockspaceID,
        ImVec2(0.0f, 0.0f),
        ImGuiDockNodeFlags_PassthruCentralNode
    );

  
    if (!dockspaceInitialized)
    {
        // Dockspace node must exist before DockBuilder touches it
        if (ImGui::DockBuilderGetNode(dockspaceID) == nullptr)
        {
            ImGui::End();
            return; // try again next frame
        }

        dockspaceInitialized = true;

        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->Size);

        ImGuiID dockMain = dockspaceID;
        ImGuiID dockLeft = 0, dockRight = 0, dockBottom = 0;

        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, &dockLeft, &dockMain);
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, &dockRight, &dockMain);
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, &dockBottom, &dockMain);

        ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Details", dockRight);
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Assets", dockBottom);
        ImGui::DockBuilderDockWindow("Script Editor", dockMain);
        ImGui::DockBuilderDockWindow("Scene", dockMain);

        if (!openScripts.empty())
        {
            ImGui::DockBuilderDockWindow("Script Docs", dockRight);
        }
 

        ImGui::DockBuilderFinish(dockspaceID);
    }

    ImGui::End();
}

void Editor::TogglePlayMode()
{
    if (engineMode == EngineMode::Editor)
    {
        std::cout << "[Editor] Entering Play Mode...\n";
		EnterPlayMode();
    }
    else
    {
		std::cout << "[Editor] Exiting Play Mode...\n";
		ExitPlayMode();
    }
}

void Editor::EnterPlayMode()
{
    if (!scriptSystem)
    {
        std::cerr << "[Editor] ScriptSystem pointer is NULL\n";
        return;
    }

	inputSystem->SetGameplayEnabled(true);

    const auto& project = ProjectManager::GetActive();

    SceneSerializer::Save(project.scenePath.string(), *entityMgr, *compMgr, meta);

    // 1. Save editor scene to memory (or temp file)
    SceneSerializer::Save("__temp_play_scene.scene", *entityMgr, *compMgr, meta);

    // 2. Clear current ECS
    entityMgr->Clear();
    compMgr->Clear();
    meta.Clear();

    // 3. Load the temp scene as runtime state
    SceneSerializer::Load("__temp_play_scene.scene", *entityMgr, *compMgr, meta);

    engineMode = EngineMode::Play;

    int scriptCount = 0;
    std::cout << std::filesystem::current_path() << "\n";


    for (EntityID id = 0; id < entityMgr->GetMaxEntities(); ++id)
    {
        Entity e{ id };
        if (!entityMgr->IsAlive(e)) continue;
        if (compMgr->HasComponent<ScriptComponent>(e))
            scriptCount++;
    }

    std::cout << "[ScriptSystem] Script components found: "
        << scriptCount << "\n";

    for (EntityID id = 0; id < entityMgr->GetMaxEntities(); ++id)
    {
        Entity e{ id };
        if (!entityMgr->IsAlive(e)) continue;
        if (!compMgr->HasComponent<ScriptComponent>(e)) continue;

        auto& sc = compMgr->GetComponent<ScriptComponent>(e);

        if (!sc.ScriptPath.empty())
            scriptSystem->LoadScript(sc);
    }
}

void Editor::ExitPlayMode()
{
    // Clear runtime state
    entityMgr->Clear();
    compMgr->Clear();
    meta.Clear();

    const auto& project = ProjectManager::GetActive();

    // Reload editor scene
    SceneSerializer::Load(
        project.scenePath.string(),
        *entityMgr,
        *compMgr,
        meta
    );

	inputSystem->SetGameplayEnabled(false);
    engineMode = EngineMode::Editor;
}

void Editor::CreateLuaScript(const std::string& scriptName, ScriptTemplate templateType)
{
    if (!ProjectManager::HasActiveProject())
        return;

    if (!IsValidLuaScriptName(scriptName))
    {
        EditorConsole::Error(
            "[Script] Invalid script name. Use letters, numbers, _ or - only."
        );
        return;
    }

    const auto& project = ProjectManager::GetActive();

    std::filesystem::path scriptsDir =
        project.rootPath / "Scripts";

    std::filesystem::create_directories(scriptsDir);

    std::filesystem::path scriptPath =
        scriptsDir / (scriptName + ".lua");

    //  Duplicate protection
    if (std::filesystem::exists(scriptPath))
    {
        EditorConsole::Error(
            "[Script] Script already exists: " + scriptName + ".lua"
        );
        return;
    }

    std::ofstream out(scriptPath);
    if (!out.is_open())
    {
        EditorConsole::Error(
            "[Script] Failed to create script file."
        );
        return;
    }

	out << GetLuaTemplateText(templateType, scriptName);

    out.close();

    // Auto-assign to selected entity
    if (selectedEntity && compMgr->HasComponent<ScriptComponent>(selectedEntity))
    {
        auto& sc = compMgr->GetComponent<ScriptComponent>(selectedEntity);
        sc.ScriptPath = "Scripts/" + scriptName + ".lua";
    }

    EditorConsole::Log(
        "[Script] Created: Scripts/" + scriptName + ".lua"
    );

    OpenScriptFile(scriptPath.string());
    FocusScriptEditor();
}


void Editor::OpenScriptFile(const std::string& path)
{
    // Already open?
    for (size_t i = 0; i < openScripts.size(); ++i)
    {
        if (openScripts[i].path == path)
        {
            activeScriptIndex = (int)i;
            return;
        }
    }

    OpenScript script;
    script.path = path;
    script.contents = ReadFileText(path);
    script.buffer.resize(script.contents.size() + 1024);
    memcpy(script.buffer.data(), script.contents.c_str(), script.contents.size());
    script.buffer[script.contents.size()] = '\0';
    script.dirty = false;

    openScripts.push_back(script);
    activeScriptIndex = (int)openScripts.size() - 1;
}

void Editor::SaveScript(OpenScript& script)
{
    WriteFileText(script.path, script.contents);
    script.dirty = false;

    std::cout << "[Editor] Saved script: " << script.path << "\n";
}

int Editor::FindOrOpenScript(const std::string& path)
{
    // Already open?
    for (int i = 0; i < (int)openScripts.size(); ++i)
    {
        if (openScripts[i].path == path)
            return i;
    }

    // Not open → load it
    OpenScript script;
    script.path = path;
    script.contents = ReadFileText(path);

    script.buffer.resize(script.contents.size() + 1024);
    memcpy(script.buffer.data(), script.contents.c_str(), script.contents.size());
    script.buffer[script.contents.size()] = '\0';

    script.dirty = false;
    script.hasError = false;
    script.errorLogged = false;
    script.highlightLine = -1;

    openScripts.push_back(script);
    return (int)openScripts.size() - 1;
}


void Editor::DrawScriptEditor()
{
    ImGui::Begin("Script Editor");

    if (scriptJump.pending)
    {
        int index = FindOrOpenScript(scriptJump.scriptPath);
        activeScriptIndex = index;

        openScripts[index].highlightLine = scriptJump.line;

        scriptJump.pending = false;
    }

    if (activeScriptIndex >= 0)
    {
        OpenScript& active = openScripts[activeScriptIndex];

        ImGui::TextUnformatted(active.path.c_str());
        ImGui::SameLine();

        if (ImGui::Button("Save"))
            SaveScript(active);

        ImGui::SameLine();

        if (ImGui::Button("Reload from Disk"))
        {
            active.contents = ReadFileText(active.path);
            active.buffer.resize(active.contents.size() + 1024);
            memcpy(active.buffer.data(), active.contents.c_str(), active.contents.size());
            active.buffer[active.contents.size()] = '\0';
            active.dirty = false;
        }
    }

    if (scriptSystem != nullptr && !scriptSystem->GetLastReloadMessage().empty())
    {
        const ImVec4 statusColor = scriptSystem->WasLastReloadSuccessful()
            ? ImVec4(0.35f, 1.0f, 0.45f, 1.0f)
            : ImVec4(1.0f, 0.4f, 0.35f, 1.0f);
        ImGui::TextColored(statusColor, "%s", scriptSystem->GetLastReloadMessage().c_str());
    }


    ImGui::Separator();

    if (openScripts.empty())
    {
        ImGui::TextDisabled("No script open.");
        ImGui::End();
        return;
    }


    if (ImGui::BeginTabBar("ScriptTabs"))
    {
        for (int i = 0; i < (int)openScripts.size(); ++i)
        {
            OpenScript& script = openScripts[i];

            std::string tabName =
                std::filesystem::path(script.path).filename().string();

            if (script.dirty)
                tabName += "*";

            bool open = true;
            if (ImGui::BeginTabItem(tabName.c_str(), &open))
            {
                activeScriptIndex = i;

                // Ctrl+S
                if (ImGui::GetIO().KeyCtrl &&
                    ImGui::IsKeyPressed(ImGuiKey_S))
                {
                    SaveScript(script);
                }

                ImGui::Separator();

                if (!pendingScriptInsert.empty())
                {
                    InsertTextAtEnd(script, pendingScriptInsert);
                    pendingScriptInsert.clear();
                }

                // ---------------- Text editor ----------------
                if (ImGui::InputTextMultiline(
                    "##ScriptText",
                    script.buffer.data(),
                    script.buffer.size(),
                    ImVec2(-1, -1),
                    ImGuiInputTextFlags_AllowTabInput |
                    ImGuiInputTextFlags_CallbackAlways,
                    ScriptEditorCallback,
                    this
                ))
                {
                    if (script.highlightLine > 0)
                    {
                        int targetLine = script.highlightLine - 1;

                        float lineHeight = ImGui::GetTextLineHeight();
                        float scrollY = targetLine * lineHeight;

                        ImGui::SetScrollY(scrollY);

                        // Visual hint (minimal, non-invasive)
                        ImGui::Separator();
                        ImGui::TextColored(
                            ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                            " Error on line %d",
                            script.highlightLine
                        );

                        script.highlightLine = -1; // one-shot
                    }
                    script.contents = script.buffer.data();
                    script.dirty = true;

                    //  REALTIME VALIDATION
                    std::string error;
                    bool ok = scriptSystem->ValidateScriptText(
                        script.contents,
                        error
                    );

                    script.hasError = !ok;
                    script.lastError = error;
                }

                int cursor = scriptCursorPos;
                std::string line =
                    GetCurrentLine(script.buffer.data(), cursor);

                if (ImGui::IsItemActive())
                {

                    const auto& api = GetScriptAPI();

                    for (const auto& category : api)
                    {
                        if (line.find(category.name + ".") != std::string::npos)
                        {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s API", category.name.c_str());
                            ImGui::Separator();

                            for (const auto& fn : category.functions)
                            {
                                ImGui::Text("%s", fn.signature.c_str());
                                ImGui::TextDisabled("%s", fn.description.c_str());
                                if (!fn.example.empty())
                                {
                                    ImGui::TextColored(
                                        ImVec4(0.65f, 0.85f, 1.0f, 1.0f),
                                        "Example: %s",
                                        fn.example.c_str());
                                }
                                ImGui::Separator();
                            }

                            ImGui::EndTooltip();
                            break;
                        }
                    }
                }

                // ---------------- Inline error UI ----------------
                if (script.hasError)
                {
                    ImGui::Separator();
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                        "Lua Error:"
                    );
                    ImGui::TextWrapped("%s", script.lastError.c_str());
                }

                ImGui::EndTabItem();
            }

            if (!open)
            {
                openScripts.erase(openScripts.begin() + i);
                if (activeScriptIndex >= i)
                    activeScriptIndex--;
                break;
            }
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}


void Editor::DrawCreateScriptPopup()
{
    if (!ImGui::BeginPopupModal(
        "Create Lua Script",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::Text("Script name:");
    ImGui::InputText(
        "##ScriptName",
        newScriptName,
        sizeof(newScriptName));

    ImGui::Spacing();

    // ---------- Template selector ----------
    static const char* templateNames[] =
    {
        "Empty",
        "Player Movement",
        "Camera Follow",
        "Simple AI",
        "Rotator",
        "Basic Mover",
        "Trigger / Interactable",
        "Audio Trigger",
        "Light Pulse",
        "Simple Pickup"
    };

    int templateIndex = (int)selectedScriptTemplate;

    ImGui::Text("Template:");
    if (ImGui::Combo(
        "##ScriptTemplate",
        &templateIndex,
        templateNames,
        IM_ARRAYSIZE(templateNames)))
    {
        selectedScriptTemplate =
            (ScriptTemplate)templateIndex;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ---------- Buttons ----------
    if (ImGui::Button("Create", ImVec2(120, 0)))
    {
        if (strlen(newScriptName) > 0)
        {
            CreateLuaScript(
                newScriptName,
                selectedScriptTemplate
            );

            newScriptName[0] = '\0';
            selectedScriptTemplate = ScriptTemplate::Empty;

            ImGui::CloseCurrentPopup();
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0)))
    {
        newScriptName[0] = '\0';
        selectedScriptTemplate = ScriptTemplate::Empty;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Editor::DrawConsoleWindow()
{
    ImGui::Begin("Console");

    if (ImGui::Button("Clear"))
    {
        EditorConsole::Clear();
    }

    ImGui::Separator();

    ImGui::BeginChild("ConsoleScroll",
        ImVec2(0, 0),
        false,
        ImGuiWindowFlags_AlwaysVerticalScrollbar);

    const auto& messages = EditorConsole::GetMessages();

    for (const ConsoleMessage& msg : messages)
    {
        if (msg.level == LogLevel::Error)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.3f, 0.3f, 1));

        if (msg.hasScriptLocation && !msg.scriptPath.empty() && msg.scriptPath[0] != '<')
        {
            if (ImGui::Selectable(msg.text.c_str()))
            {
                scriptJump.pending = true;
                scriptJump.scriptPath = msg.scriptPath;
                scriptJump.line = msg.line;
            }
        }
        else
        {
            ImGui::TextWrapped("%s", msg.text.c_str());
        }

        if (msg.level == LogLevel::Error)
            ImGui::PopStyleColor();
    }

    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

void Editor::DrawProjectSettingsPanel()
{
    ImGui::Begin("Project Settings");

    if (!ProjectManager::HasActiveProject())
    {
        ImGui::TextDisabled("No active project.");
        ImGui::End();
        return;
    }

    const auto& project = ProjectManager::GetActive();
    static std::filesystem::path cachedProjectFile;
    static char startupSceneBuffer[260] = {};
    static char outputFolderBuffer[260] = {};
    static char runtimeIdBuffer[128] = {};

    if (cachedProjectFile != project.projectFile)
    {
        cachedProjectFile = project.projectFile;
        std::error_code startupSceneEc;
        const auto startupSceneRelative = std::filesystem::relative(project.startupScenePath, project.rootPath, startupSceneEc);
        const std::string startupSceneString = startupSceneEc
            ? project.startupScenePath.generic_string()
            : startupSceneRelative.generic_string();
        strncpy_s(startupSceneBuffer, startupSceneString.c_str(), sizeof(startupSceneBuffer) - 1);
        strncpy_s(outputFolderBuffer, project.buildOutputPath.generic_string().c_str(), sizeof(outputFolderBuffer) - 1);
        strncpy_s(runtimeIdBuffer, project.runtimeIdentifier.c_str(), sizeof(runtimeIdBuffer) - 1);
    }

    ImGui::TextDisabled("Project: %s", project.name.c_str());
    ImGui::InputText("Startup Scene", startupSceneBuffer, IM_ARRAYSIZE(startupSceneBuffer));
    if (ImGui::Button("Use Current Scene"))
    {
        std::error_code currentSceneEc;
        const auto currentSceneRelative = std::filesystem::relative(project.scenePath, project.rootPath, currentSceneEc);
        const std::string currentSceneString = currentSceneEc
            ? project.scenePath.generic_string()
            : currentSceneRelative.generic_string();
        strncpy_s(startupSceneBuffer, currentSceneString.c_str(), sizeof(startupSceneBuffer) - 1);
    }

    ImGui::InputText("Build Output Folder", outputFolderBuffer, IM_ARRAYSIZE(outputFolderBuffer));
    ImGui::InputText("Runtime Identifier", runtimeIdBuffer, IM_ARRAYSIZE(runtimeIdBuffer));

    if (ImGui::Button("Save Build Settings"))
    {
        const std::filesystem::path startupScene = std::filesystem::path(startupSceneBuffer).is_absolute()
            ? std::filesystem::path(startupSceneBuffer)
            : project.rootPath / startupSceneBuffer;
        const std::filesystem::path buildOutput = std::filesystem::path(outputFolderBuffer).is_absolute()
            ? std::filesystem::path(outputFolderBuffer)
            : project.rootPath / outputFolderBuffer;

        ProjectManager::UpdateBuildSettings(startupScene, buildOutput, runtimeIdBuffer);
        cachedProjectFile.clear();
        EditorConsole::Log("[Project] Build settings updated.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Build Prototype"))
    {
        PackageProject();
    }

    ImGui::Separator();
    ImGui::TextWrapped("Packaged builds resolve runtime paths relative to Game.exe, so they remain double-clickable even outside the engine folder.");
    ImGui::End();
}

void Editor::DrawScriptDocsPanel()
{
    ImGui::Begin("Script Docs");

    static char searchBuf[128] = {};
    ImGui::InputTextWithHint(
        "##ScriptDocSearch",
        "Search API (e.g. Translate, Transform, print)...",
        searchBuf,
        sizeof(searchBuf)
    );

    ImGui::Separator();

    std::string query = searchBuf;
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    const auto& api = GetScriptAPI();

    for (const auto& category : api)
    {
        // Check if category should be shown
        bool categoryMatches =
            query.empty() ||
            category.name.find(query) != std::string::npos ||
            category.description.find(query) != std::string::npos;

        bool hasVisibleFunctions = false;

        for (const auto& fn : category.functions)
        {
            std::string sig = fn.signature;
            std::string desc = fn.description;

            std::transform(sig.begin(), sig.end(), sig.begin(), ::tolower);
            std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);

            if (query.empty() ||
                sig.find(query) != std::string::npos ||
                desc.find(query) != std::string::npos)
            {
                hasVisibleFunctions = true;
                break;
            }
        }

        if (!categoryMatches && !hasVisibleFunctions)
            continue;

        if (ImGui::CollapsingHeader(
            category.name.c_str(),
            ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (!category.description.empty())
            {
                ImGui::TextDisabled("%s", category.description.c_str());
                ImGui::Separator();
            }

            for (const auto& fn : category.functions)
            {
                std::string sig = fn.signature;
                std::string desc = fn.description;

                std::transform(sig.begin(), sig.end(), sig.begin(), ::tolower);
                std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);

                if (!query.empty() &&
                    sig.find(query) == std::string::npos &&
                    desc.find(query) == std::string::npos)
                    continue;

                if (ImGui::Selectable(fn.signature.c_str()))
                {
                    pendingScriptInsert = fn.signature;
                    FocusScriptEditor();
                }

                ImGui::Indent();
                ImGui::TextWrapped("%s", fn.description.c_str());
                if (!fn.example.empty())
                {
                    ImGui::TextColored(
                        ImVec4(0.65f, 0.85f, 1.0f, 1.0f),
                        "Example: %s",
                        fn.example.c_str());
                }
                ImGui::Unindent();
                ImGui::Separator();
            }
        }
    }

    ImGui::End();
}


void Editor::FocusScriptEditor()
{
    ImGui::SetWindowFocus("Script Editor");
}

void Editor::DrawInputDebug(InputSystem& input)
{
    if (!showInputDebug)
        return;

    ImGui::Begin("Input Debug", &showInputDebug);

    ImGui::Text("Actions:");
    ImGui::Separator();

    for (const auto& [name, action] : input.GetActions())
    {
        const char* stateStr = "None";

        switch (action.state)
        {
        case InputActionState::Pressed:  stateStr = "Pressed"; break;
        case InputActionState::Held:     stateStr = "Held";    break;
        case InputActionState::Released: stateStr = "Released"; break;
        default: break;
        }

        ImGui::Text("%s : %s", name.c_str(), stateStr);
    }

    ImGui::Separator();
    ImGui::Text("Mouse DX: %.2f", input.GetMouseDX());
    ImGui::Text("Mouse DY: %.2f", input.GetMouseDY());

    ImGui::End();
}
