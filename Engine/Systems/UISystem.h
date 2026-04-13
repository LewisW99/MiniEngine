#pragma once

#include <imgui.h>
#include <string>
#include "../Components/RuntimeUIComponent.h"
#include "../ECS/ComponentManager.h"
#include "../ECS/EntityManager.h"
#include "../EditorConsole.h"
#include "../EventBus.h"
#include "../Rendering/ResourceManager.h"

class UISystem
{
public:
    struct RuntimeUIRect
    {
        ImVec2 position{};
        ImVec2 size{};
    };

    static void Draw(const EntityManager& entities, const ComponentManager& components, const int screenWidth, const int screenHeight)
    {
        DrawRegion(entities, components, ImVec2(0.0f, 0.0f), screenWidth, screenHeight, false);
    }

    static bool DrawInRegion(
        const EntityManager& entities,
        const ComponentManager& components,
        const ImVec2& origin,
        const int screenWidth,
        const int screenHeight)
    {
        return DrawRegion(entities, components, origin, screenWidth, screenHeight, true);
    }

private:
    static bool DrawRegion(
        const EntityManager& entities,
        const ComponentManager& components,
        const ImVec2& origin,
        const int screenWidth,
        const int screenHeight,
        const bool embedInCurrentWindow)
    {
        bool consumedInput = false;
        for (uint32_t id = 0; id < entities.GetMaxEntities(); ++id)
        {
            const Entity entity{ id };
            if (!entities.IsAlive(entity) || !components.HasComponent<RuntimeUIComponent>(entity))
            {
                continue;
            }

            const auto& element = components.GetComponent<RuntimeUIComponent>(entity);
            if (!element.visible || !element.isScreenUI)
            {
                continue;
            }

            if (element.isCanvasRoot || element.type == RuntimeUIElementType::Canvas)
            {
                continue;
            }

            const RuntimeUIRect rect = GetRectForEditor(element, screenWidth, screenHeight);
            const ImVec2 screenPosition(origin.x + rect.position.x, origin.y + rect.position.y);

            if (embedInCurrentWindow)
            {
                ImGui::PushID(static_cast<int>(entity.id));
                switch (element.type)
                {
                case RuntimeUIElementType::Canvas:
                    break;
                case RuntimeUIElementType::Text:
                {
                    ImGui::GetWindowDrawList()->AddText(screenPosition, ImGui::ColorConvertFloat4ToU32(ToImVec4(element.textColor)), element.text.c_str());
                    break;
                }
                case RuntimeUIElementType::Button:
                {
                    const ImVec2 previousCursor = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(screenPosition);
                    ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(element.buttonColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(element.buttonHoveredColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(element.buttonPressedColor));
                    ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(element.textColor));
                    if (ImGui::Button(element.text.c_str(), rect.size))
                    {
                        consumedInput = true;
                        PublishButtonEvent(entity, element);
                    }
                    ImGui::PopStyleColor(4);
                    ImGui::SetCursorScreenPos(previousCursor);
                    ImGui::Dummy(ImVec2(0.0f, 0.0f));
                    break;
                }
                case RuntimeUIElementType::Image:
                {
                    const ImVec2 previousCursor = ImGui::GetCursorScreenPos();
                    ImGui::SetCursorScreenPos(screenPosition);
                    const GLuint textureId = ResourceManager::GetTexture(element.texturePath);
                    if (textureId != 0)
                    {
                        ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(textureId)), rect.size);
                    }
                    else
                    {
                        ImGui::TextWrapped("%s", element.texturePath.c_str());
                    }
                    ImGui::SetCursorScreenPos(previousCursor);
                    ImGui::Dummy(ImVec2(0.0f, 0.0f));
                    break;
                }
                }
                ImGui::PopID();
            }
            else
            {
                const std::string windowName = "RuntimeUI_" + std::to_string(entity.id);

                ImGui::SetNextWindowPos(screenPosition);
                ImGui::SetNextWindowSize(rect.size);
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
                case RuntimeUIElementType::Canvas:
                    break;
                case RuntimeUIElementType::Text:
                    ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(element.textColor));
                    ImGui::TextWrapped("%s", element.text.c_str());
                    ImGui::PopStyleColor();
                    break;
                case RuntimeUIElementType::Button:
                    ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(element.buttonColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(element.buttonHoveredColor));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(element.buttonPressedColor));
                    ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(element.textColor));
                    if (ImGui::Button(element.text.c_str(), rect.size))
                    {
                        consumedInput = true;
                        PublishButtonEvent(entity, element);
                    }
                    ImGui::PopStyleColor(4);
                    break;
                case RuntimeUIElementType::Image:
                {
                    const GLuint textureId = ResourceManager::GetTexture(element.texturePath);
                    if (textureId != 0)
                    {
                        ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(textureId)), rect.size);
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

        return consumedInput;
    }

    static void PublishButtonEvent(const Entity entity, const RuntimeUIComponent& element)
    {
        if (element.buttonEvent.empty())
        {
            EditorConsole::Warn("[UI] Button '" + element.text + "' clicked without an assigned event.");
            return;
        }

        if (element.luaFunction.empty())
        {
            EditorConsole::Warn("[UI] Button '" + element.text + "' clicked without a bound Lua function.");
        }

        EventBus::Publish({ element.buttonEvent, entity.id, element.text, element.luaFunction });
    }

    static ImVec4 ToImVec4(const RuntimeUIColor& color)
    {
        return ImVec4(color.r, color.g, color.b, color.a);
    }

    static RuntimeUIRect GetRect(const RuntimeUIComponent& element, const int screenWidth, const int screenHeight)
    {
        const ImVec2 size(element.width, element.height);
        return { ResolvePosition(element.anchor, size, screenWidth, screenHeight, element.offsetX, element.offsetY), size };
    }

public:
    static RuntimeUIRect GetRectForEditor(const RuntimeUIComponent& element, const int screenWidth, const int screenHeight)
    {
        return GetRect(element, screenWidth, screenHeight);
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
