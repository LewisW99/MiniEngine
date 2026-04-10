#pragma once

#include <imgui.h>
#include "../Components/RuntimeUIComponent.h"
#include "../ECS/ComponentManager.h"
#include "../ECS/EntityManager.h"
#include "../EventBus.h"
#include "../Rendering/ResourceManager.h"

class UISystem
{
public:
    static void Draw(const EntityManager& entities, const ComponentManager& components, const int screenWidth, const int screenHeight)
    {
        for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
        {
            const Entity entity{ id };
            if (!entities.IsAlive(entity) || !components.HasComponent<RuntimeUIComponent>(entity))
            {
                continue;
            }

            const auto& element = components.GetComponent<RuntimeUIComponent>(entity);
            if (!element.visible)
            {
                continue;
            }

            const ImVec2 size(element.width, element.height);
            const ImVec2 position = ResolvePosition(element.anchor, size, screenWidth, screenHeight, element.offsetX, element.offsetY);
            const std::string windowName = "RuntimeUI_" + std::to_string(entity.id);

            ImGui::SetNextWindowPos(position);
            ImGui::SetNextWindowSize(size);
            ImGui::SetNextWindowBgAlpha(0.2f);

            const ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoBringToFrontOnFocus;

            if (!ImGui::Begin(windowName.c_str(), nullptr, flags))
            {
                ImGui::End();
                continue;
            }

            switch (element.type)
            {
            case RuntimeUIElementType::Text:
                ImGui::TextWrapped("%s", element.text.c_str());
                break;
            case RuntimeUIElementType::Button:
                if (ImGui::Button(element.text.c_str(), size))
                {
                    EventBus::Publish({ element.buttonEvent, entity.id, element.text, {} });
                }
                break;
            case RuntimeUIElementType::Image:
            {
                const GLuint textureId = ResourceManager::GetTexture(element.texturePath);
                if (textureId != 0)
                {
                    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(textureId)), size);
                }
                else
                {
                    ImGui::TextWrapped("%s", element.texturePath.c_str());
                }
                break;
            }
            }

            ImGui::End();
        }
    }

private:
    static ImVec2 ResolvePosition(
        const RuntimeUIAnchor anchor,
        const ImVec2& size,
        const int screenWidth,
        const int screenHeight,
        const float offsetX,
        const float offsetY)
    {
        switch (anchor)
        {
        case RuntimeUIAnchor::TopCenter:
            return { (screenWidth - size.x) * 0.5f + offsetX, offsetY };
        case RuntimeUIAnchor::TopRight:
            return { screenWidth - size.x - offsetX, offsetY };
        case RuntimeUIAnchor::Center:
            return { (screenWidth - size.x) * 0.5f + offsetX, (screenHeight - size.y) * 0.5f + offsetY };
        case RuntimeUIAnchor::BottomLeft:
            return { offsetX, screenHeight - size.y - offsetY };
        case RuntimeUIAnchor::BottomCenter:
            return { (screenWidth - size.x) * 0.5f + offsetX, screenHeight - size.y - offsetY };
        case RuntimeUIAnchor::BottomRight:
            return { screenWidth - size.x - offsetX, screenHeight - size.y - offsetY };
        case RuntimeUIAnchor::TopLeft:
        default:
            return { offsetX, offsetY };
        }
    }
};
