#pragma once

#include "Core/ECS.hpp"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Wild
{
    // Flags the renderer is free to read, the Debug panel writes them.
    struct EditorDebugFlags
    {
        bool wireframe = false;
        bool showColliders = false;
        bool showNormals = false;
        bool freezeCulling = false;
        bool masterDebugDraw = true;
    };

    // State shared between panels (selection, gizmo settings, viewport rect).
    struct EditorState
    {
        Entity selectedEntity = entt::null;

        ImGuizmo::OPERATION gizmoOperation = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE gizmoMode = ImGuizmo::WORLD;

        ImVec2 viewportPos{};
        ImVec2 viewportSize{};
        bool viewportHovered = false;

        EditorDebugFlags debug{};
    };
} // namespace Wild
