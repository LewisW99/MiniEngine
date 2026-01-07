
#include "Editor.h"
#include <string>
#include <algorithm>
#include "../Engine/ImGuizmo.h"
#include <SDL2/SDL.h>
#include "../Engine/SceneSerializer.h"
#include "../Engine/AssetDatabase/AssetImporter.h"
#include <filesystem>
#include <windows.h>
#pragma comment(lib, "Comdlg32.lib")

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

Editor::Editor(EntityManager* entityMgr,
    ComponentManager* compMgr,
    Renderer* renderer,
    Camera* camera,
    StreamingManager* streamer)
    : entityMgr(entityMgr),
    compMgr(compMgr),
    renderer(renderer),
    camera(camera),
    streamer(streamer)
{
}

// Editor Drawing
void Editor::Draw()
{

    ImGuiIO& io = ImGui::GetIO();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
    {
        SceneSerializer::Save("scene.json", *entityMgr, *compMgr, meta);
        statusText = "Scene Saved!";
        statusTimer = 2.0f; // show for 2 seconds
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
    {
        SceneSerializer::Load("scene.json", *entityMgr, *compMgr, meta);
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
                SceneSerializer::Save("scene.json", *entityMgr, *compMgr, meta);

            if (ImGui::MenuItem("Load Scene", "Ctrl+O"))
                SceneSerializer::Load("scene.json", *entityMgr, *compMgr, meta);

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

        ImGui::EndMainMenuBar();
    }


    DrawHierarchy();
    DrawSceneView();
    DrawDetails();
	DrawAssetsPanel();
}

// scene view panel (center)
void Editor::DrawSceneView()
{
    ImGui::SetNextWindowPos(ImVec2(300, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(1320, 740), ImGuiCond_Once);
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize;


    ImGui::Begin("Scene", nullptr, flags);

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
    ImGui::SetNextWindowPos(ImVec2(10, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(280, 740), ImGuiCond_Once);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize;

    ImGui::Begin("Hierarchy", nullptr, flags);

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
    ImGui::SetNextWindowPos(ImVec2(1640, 40), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(270, 740), ImGuiCond_Once);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize;


        ImGui::Begin("Details", nullptr, flags);

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



    ImGui::End();
}

#include <filesystem>
#include <Windows.h>
#pragma comment(lib, "Comdlg32.lib")

void Editor::DrawAssetsPanel()
{

    ImGui::SetNextWindowPos(ImVec2(10, 790), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(1900, 280), ImGuiCond_Once);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    ImGui::Begin("Assets", nullptr, flags);

    ImGui::TextDisabled("Project Assets");
    ImGui::Separator();

    static bool showImportPopup = false;
    if (ImGui::Button("➕ Import Asset", ImVec2(200, 30)))
        ImGui::OpenPopup("Import Asset");

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
                assetDB.Scan("Assets");
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
    if (!scanned) { assetDB.Scan("Assets"); scanned = true; }

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



