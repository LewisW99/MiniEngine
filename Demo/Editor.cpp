
#include "Editor.h"
#include <string>
#include <algorithm>
#include "../Engine/ImGuizmo.h"
#include <SDL2/SDL.h>
#include "../Engine/SceneSerializer.h"
#include "../Engine/AssetDatabase/AssetImporter.h"
#include <filesystem>
#include <windows.h>
#include <imgui_internal.h>
#include <imgui.h>
#include "Editor/Managers/ProjectManager.h"
#include "../Engine/Components/Physics/PhysicsComponent.h"
#include "../Engine/Scripting/ScriptComponent.h"
#include <sstream>
#include "../Engine/EditorConsole.h"
#include "Scripting/ScriptAPI.h"

#pragma comment(lib, "Comdlg32.lib")


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
            "-- Rotation placeholder\n\n"
            "local rotationSpeed = 45.0\n\n"
            "function OnUpdate(self, dt)\n"
            "    -- Rotation API coming later\n"
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



Editor::Editor(EntityManager* entityMgr,
    ComponentManager* compMgr,
    Renderer* renderer,
    Camera* camera,
    StreamingManager* streamer,
    ScriptSystem* scripting)
    : entityMgr(entityMgr),
    compMgr(compMgr),
    renderer(renderer),
    camera(camera),
    streamer(streamer),
    scriptSystem(scripting)
{
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

// Editor Drawing
void Editor::Draw()
{
	

    if(!ProjectManager::HasActiveProject())
		return;


    const auto& project = ProjectManager::GetActive();
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
                // Clear everything
                for (uint32_t id = 0; id < 10000; ++id)
                {
                    Entity e{ id };
                    if (entityMgr->IsAlive(e))
                    {
                        compMgr->RemoveComponent<TransformComponent>(e);
                        entityMgr->DestroyEntity(e);
                        meta.Remove(e);
                    }
                }
                selectedEntity = {};
            }

            if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
                SceneSerializer::Save(project.scenePath.string(), *entityMgr, *compMgr, meta);

            if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
                SceneSerializer::Load(project.scenePath.string(), *entityMgr, *compMgr, meta);

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
                compMgr->AddComponent(e, t);
                meta.SetName(e, "Entity " + std::to_string(e.id));
                selectedEntity = e;
                renamingEntity = e;
            }

            if (selectedEntity && entityMgr->IsAlive(selectedEntity))
            {
                if (ImGui::MenuItem("Duplicate Selected"))
                {
                    Entity clone = entityMgr->CreateEntity();
                    auto t = compMgr->GetComponent<TransformComponent>(selectedEntity);
                    compMgr->AddComponent(clone, t);
                    meta.SetName(clone, meta.GetName(selectedEntity) + " (1)");
                }

                if (ImGui::MenuItem("Delete Selected"))
                {
                    compMgr->RemoveComponent<TransformComponent>(selectedEntity);
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
	DrawAssetsPanel();
    DrawScriptEditor();
    DrawCreateScriptPopup();
	DrawConsoleWindow();
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
        renderer->RenderToTexture(*entityMgr, *compMgr, *camera, texW, texH, selectedEntity);
        ImTextureID tex = (ImTextureID)(intptr_t)renderer->GetSceneTextureID();

        if (tex)
            ImGui::Image(tex, avail, ImVec2(0, 1), ImVec2(1, 0));
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

            for (uint32_t id = 0; id < 10000; ++id)
            {
                Entity e{ id };
                if (!entityMgr->IsAlive(e)) continue;

                try {
                    const auto& t = compMgr->GetComponent<TransformComponent>(e);
                    glm::vec3 pos(t.position.x, t.position.y, t.position.z);
                    glm::vec3 half(0.5f * t.scale.x, 0.5f * t.scale.y, 0.5f * t.scale.z);

                    glm::vec3 aabbMin = pos - half;
                    glm::vec3 aabbMax = pos + half;

                    float tHit;
                    if (RayIntersectsAABB(rayOrigin, rayDir, aabbMin, aabbMax, tHit)) {
                        if (tHit < closestT && tHit > 0.0f) {
                            closestT = tHit;
                            closestEntity = e;
                        }
                    }
                }
                catch (...) {}
            }

            //  Select or deselect
            if (closestEntity) selectedEntity = closestEntity;
            else selectedEntity = {};
        }
    }

    //ImGuizmo Manipulation
    if (selectedEntity && entityMgr->IsAlive(selectedEntity))
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
        static ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
        static ImGuizmo::MODE gizmoMode = ImGuizmo::WORLD;
        static bool snapEnabled = false;
        static float snapValues[3] = { 1.0f, 1.0f, 1.0f };

        // Cycle R → Translate / Rotate / Scale
        if (ImGui::IsKeyPressed(ImGuiKey_R)) {
            if (gizmoOperation == ImGuizmo::TRANSLATE)
                gizmoOperation = ImGuizmo::ROTATE;
            else if (gizmoOperation == ImGuizmo::ROTATE)
                gizmoOperation = ImGuizmo::SCALE;
            else
                gizmoOperation = ImGuizmo::TRANSLATE;
        }

        // L toggles Local/World
        if (ImGui::IsKeyPressed(ImGuiKey_L))
            gizmoMode = (gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

        // Ctrl = snapping
        snapEnabled = ImGui::GetIO().KeyCtrl;
		renderer->snapGridVisible = snapEnabled;
        renderer -> snapStep = snapValues[0];
        switch (gizmoOperation) {
        case ImGuizmo::TRANSLATE: snapValues[0] = snapValues[1] = snapValues[2] = 1.0f; break;
        case ImGuizmo::ROTATE:    snapValues[0] = snapValues[1] = snapValues[2] = 15.0f; break;
        case ImGuizmo::SCALE:     snapValues[0] = snapValues[1] = snapValues[2] = 0.1f; break;
        }

        // Disable camera control while dragging
        if (ImGuizmo::IsUsing())
            SDL_SetRelativeMouseMode(SDL_FALSE);
        else if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsOver())
            SDL_SetRelativeMouseMode(SDL_FALSE);

    

        // Manipulate
        if (ImGuizmo::Manipulate(&view[0][0], &proj[0][0],
            gizmoOperation, gizmoMode,
            &model[0][0],
            nullptr,
            snapEnabled ? snapValues : nullptr))
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
            gizmoOperation == ImGuizmo::TRANSLATE ? "Translate" :
            gizmoOperation == ImGuizmo::ROTATE ? "Rotate" : "Scale",
            gizmoMode == ImGuizmo::WORLD ? "World" : "Local",
            snapEnabled ? "ON (Ctrl)" : "OFF");
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
        ImGui::PopStyleColor();
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
        t.position = { 0, 0, 0 };
        t.rotation = { 0, 0, 0 };
        t.scale = { 1, 1, 1 };
        compMgr->AddComponent(e, t);

        std::string name = "Entity " + std::to_string(e.id);
        meta.SetName(e, name);

        selectedEntity = e;
        renamingEntity = e; // immediately enter rename mode
    }

    ImGui::Separator();

    for (uint32_t id = 0; id < 10000; ++id)
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
                auto t = compMgr->GetComponent<TransformComponent>(e);
                compMgr->AddComponent(clone, t);
                std::string newName = name + " (1)";
                meta.SetName(clone, newName);
            }

            if (ImGui::MenuItem("Delete"))
            {
                compMgr->RemoveComponent<TransformComponent>(e);
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
    ImGuiIO& io = ImGui::GetIO();
    
    ImGui::Begin("Details");

    if (!entityMgr) {
        ImGui::TextDisabled("Error: entityMgr is null!");
        ImGui::End();
        return;
    }

    if (selectedEntity && entityMgr->IsAlive(selectedEntity))
    {
        ImGui::Text("Entity ID: %u", selectedEntity.id);

        try
        {
            auto& transform = compMgr->GetComponent<TransformComponent>(selectedEntity);
            ImGui::SeparatorText("Transform");
            ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
            ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.1f);
            ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f, 0.01f, 10.0f);
        }
        catch (...) {
            ImGui::TextDisabled("No TransformComponent found.");
        }
    }
    else
        ImGui::Text("No entity selected.");

    if (selectedAsset)
    {
        ImGui::SeparatorText("Asset Info");
        ImGui::Text("Name: %s", selectedAsset->name.c_str());
        ImGui::Text("Path: %s", selectedAsset->path.c_str());
       // ImGui::Text("Type: %s", AssetTypeToString(selectedAsset->type));

        if (selectedAsset->type == AssetType::Texture)
        {
            ImGui::Text("Resolution: %d x %d", selectedAsset->width, selectedAsset->height);
            ImGui::Text("Channels: %d", selectedAsset->channels);
        }
    }

    bool hasPhysics = compMgr->HasComponent<PhysicsComponent>(selectedEntity);

    if (ImGui::Checkbox("Enable Physics", &hasPhysics))
    {
        if (hasPhysics)
            compMgr->AddComponent(selectedEntity, PhysicsComponent{});
        else
            compMgr->RemoveComponent<PhysicsComponent>(selectedEntity);
    }

    if (compMgr->HasComponent<ScriptComponent>(selectedEntity))
    {
        auto& sc = compMgr->GetComponent<ScriptComponent>(selectedEntity);

        ImGui::SeparatorText("Script");

        auto scripts = GetProjectLuaScripts();

        // Insert New Script option at the top
        scripts.insert(scripts.begin(), "<New Script>");

        int currentIndex = 0; // default to <New Script>
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
                    // -------- New Script --------
                    if (scripts[i] == "<New Script>")
                    {
                        requestCreateScriptPopup = true;
                    }
                    else
                    {
                        sc.ScriptPath = scripts[i];

                        const auto& project = ProjectManager::GetActive();
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

        // Existing script actions
        if (!sc.ScriptPath.empty())
        {
            if (ImGui::Button("Open Script"))
            {
                const auto& project = ProjectManager::GetActive();
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
    else if (selectedEntity)
    {
        if (ImGui::Button("Add Script"))
        {
            ScriptComponent sc;
            sc.ScriptPath = "";
            compMgr->AddComponent(selectedEntity, sc);
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
    if (ImGui::Button("➕ Import Asset", ImVec2(200, 30)))
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
                case 0: AssetImporter::ImportModel(filePath); break;
                case 1: AssetImporter::ImportTexture(filePath); break;
                case 2: AssetImporter::ImportAudio(filePath); break;
                }

                assetDB.ClearPreviews();   
                assetDB.Scan(ProjectManager::GetActive().assetPath.string());
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
    static bool scanned = false;
    if (!scanned) { assetDB.Scan(ProjectManager::GetActive().assetPath.string()); scanned = true; }

    auto& assets = assetDB.GetAssets();
    int itemsPerRow = 6;
    int itemCount = 0;

    for (auto& a : assets)
    {
        ImGui::BeginGroup();

        std::string nameLower = a.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (!query.empty() && nameLower.find(query) == std::string::npos)
            continue;

        if (a.type == AssetType::Texture)
        {
            GLuint tex = LoadTextureForPreview(a);
            if (tex)
                ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(64, 64));
            else
                ImGui::Button("🖼", ImVec2(64, 64));
        }
        else if (a.type == AssetType::Model)
            ImGui::Button("📦", ImVec2(64, 64));
        else if (a.type == AssetType::Audio)
            ImGui::Button("🎵", ImVec2(64, 64));
        else
            ImGui::Button("❓", ImVec2(64, 64));

        if (ImGui::IsItemClicked())
            selectedAsset = &a;

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
        return a.previewID; // Already loaded

    int w, h, c;
    stbi_set_flip_vertically_on_load(false); // keep upright
    unsigned char* data = stbi_load(a.path.c_str(), &w, &h, &c, 4);
    if (!data)
    {
        std::cerr << "[Thumbnail] Failed to load preview: " << a.path << "\n";
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);

    a.previewID = tex;
    a.previewLoaded = true;

    std::cout << "[Thumbnail] Loaded preview for " << a.name << " (" << w << "x" << h << ", TexID: " << tex << ")\n";

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

    // Reload editor scene
    SceneSerializer::Load(
        activeScenePath.string(),
        *entityMgr,
        *compMgr,
        meta
    );

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
        "Rotator"
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