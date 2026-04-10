#pragma once

#include "../Engine/ImGuizmo.h"

struct EditorGizmoState
{
    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE mode = ImGuizmo::WORLD;
    bool snapEnabled = false;
    float snapValues[3] = { 1.0f, 1.0f, 1.0f };
};

class EditorGizmoController
{
public:
    static void TickHotkeys(EditorGizmoState& state)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_R))
        {
            if (state.operation == ImGuizmo::TRANSLATE)
            {
                state.operation = ImGuizmo::ROTATE;
            }
            else if (state.operation == ImGuizmo::ROTATE)
            {
                state.operation = ImGuizmo::SCALE;
            }
            else
            {
                state.operation = ImGuizmo::TRANSLATE;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_L))
        {
            state.mode = state.mode == ImGuizmo::LOCAL ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        }

        state.snapEnabled = ImGui::GetIO().KeyCtrl;
        switch (state.operation)
        {
        case ImGuizmo::TRANSLATE:
            state.snapValues[0] = state.snapValues[1] = state.snapValues[2] = 1.0f;
            break;
        case ImGuizmo::ROTATE:
            state.snapValues[0] = state.snapValues[1] = state.snapValues[2] = 15.0f;
            break;
        case ImGuizmo::SCALE:
            state.snapValues[0] = state.snapValues[1] = state.snapValues[2] = 0.1f;
            break;
        default:
            break;
        }
    }
};
