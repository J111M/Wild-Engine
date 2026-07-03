#include "Editor/Panels/ViewportPanel.hpp"

#include "Core/Engine.hpp"
#include "Editor/EditorIcons.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Tools/Log.hpp"

#include <ImGuizmo.h>
#include <imgui.h>

#include <filesystem>

namespace Wild
{
    namespace
    {
        void GizmoToolButton(EditorState& state, const char* label, const char* tooltip, ImGuizmo::OPERATION op)
        {
            bool active = state.gizmoOperation == op;
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label)) state.gizmoOperation = op;
            if (active) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("%s", tooltip);
        }
    } // namespace

    void ViewportPanel::OnRender()
    {
        ImVec2 available = ImGui::GetContentRegionAvail();

        if (m_texture && available.x > 1.0f && available.y > 1.0f)
        {
            ImGui::Image(m_texture, available);

            m_state.viewportPos = ImGui::GetItemRectMin();
            m_state.viewportSize = ImGui::GetItemRectSize();
            m_state.viewportHovered = ImGui::IsItemHovered();

            // Models dragged from the Assets panel spawn in the scene
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("WILD_MODEL_ASSET"))
                    SpawnDroppedModel(static_cast<const char*>(payload->Data));

                ImGui::EndDragDropTarget();
            }
        }
        else
        {
            m_state.viewportSize = ImVec2(0.0f, 0.0f);
            m_state.viewportHovered = false;
            ImVec2 contentMin = ImGui::GetCursorStartPos();
            ImGui::SetCursorPos(ImVec2(contentMin.x + 12.0f, contentMin.y + 40.0f));
            ImGui::TextDisabled("Waiting for the renderer's composite texture...");
        }

        DrawGizmoToolbar();
    }

    void ViewportPanel::SpawnDroppedModel(const char* assetPath)
    {
        auto ecs = engine.GetECS();

        auto entity = ecs->CreateEntity();
        ecs->AddComponent<SceneObject>(entity);
        auto& transform = ecs->AddComponent<Transform>(entity, glm::vec3(0.0f), entity);
        transform.Name = std::filesystem::path(assetPath).stem().string();

        for (auto cameraEntity : ecs->View<Camera>())
        {
            auto& camera = ecs->GetComponent<Camera>(cameraEntity);
            if (camera.GetCameraIndex() != 0) continue;

            const glm::mat4& view = camera.GetView();
            glm::vec3 forward = -glm::normalize(glm::vec3(view[0][2], view[1][2], view[2][2]));
            transform.SetPosition(camera.GetPosition() + forward * 10.0f);
            break;
        }

        ecs->AddComponent<Model>(entity, std::filesystem::path(assetPath), entity);

        m_state.selectedEntity = entity;

        WD_INFO("Spawned model from asset drop: {}", assetPath);
    }

    void ViewportPanel::DrawGizmoToolbar()
    {
        ImVec2 contentMin = ImGui::GetCursorStartPos();
        ImGui::SetCursorPos(ImVec2(contentMin.x + 8.0f, contentMin.y + 8.0f));
        ImGui::BeginGroup();

        const bool icons = EditorIconsLoaded();
        GizmoToolButton(m_state, icons ? WD_ICON_MOVE : "Move", "Move (R)", ImGuizmo::TRANSLATE);
        ImGui::SameLine(0.0f, 4.0f);
        GizmoToolButton(m_state, icons ? WD_ICON_ROTATE : "Rotate", "Rotate (T)", ImGuizmo::ROTATE);
        ImGui::SameLine(0.0f, 4.0f);
        GizmoToolButton(m_state, icons ? WD_ICON_SCALE : "Scale", "Scale (Y)", ImGuizmo::SCALE);

        ImGui::EndGroup();

        if (m_state.viewportHovered && !ImGuizmo::IsUsing() && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_state.gizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_T)) m_state.gizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_Y)) m_state.gizmoOperation = ImGuizmo::SCALE;
        }
    }
} // namespace Wild
