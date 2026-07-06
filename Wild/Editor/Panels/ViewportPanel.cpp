#include "Editor/Panels/ViewportPanel.hpp"

#include "Core/Camera.hpp"
#include "Core/Engine.hpp"
#include "Core/Guid.hpp"
#include "Editor/EditorIcons.hpp"
#include "Renderer/Resources/Mesh.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Tools/Log.hpp"

#include <ImGuizmo.h>
#include <imgui.h>

#include <filesystem>
#include <limits>

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

        // Standard Möller-Trumbore ray/triangle test, world-space in, world-space out.
        bool RayIntersectsTriangle(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& a, const glm::vec3& b,
                                   const glm::vec3& c, float& outT)
        {
            constexpr float kEpsilon = 1e-6f;

            glm::vec3 edge1 = b - a;
            glm::vec3 edge2 = c - a;
            glm::vec3 h = glm::cross(dir, edge2);
            float det = glm::dot(edge1, h);
            if (std::abs(det) < kEpsilon) return false;

            float invDet = 1.0f / det;
            glm::vec3 s = origin - a;
            float u = invDet * glm::dot(s, h);
            if (u < 0.0f || u > 1.0f) return false;

            glm::vec3 q = glm::cross(s, edge1);
            float v = invDet * glm::dot(dir, q);
            if (v < 0.0f || u + v > 1.0f) return false;

            float t = invDet * glm::dot(edge2, q);
            if (t <= kEpsilon) return false;

            outT = t;
            return true;
        }

        Entity PickEntityAtRay(entt::registry& registry, const glm::vec3& rayOrigin, const glm::vec3& rayDir)
        {
            Entity closest = entt::null;
            float closestT = std::numeric_limits<float>::max();

            for (auto entity : registry.view<MeshComponent, Transform>())
            {
                auto& meshComponent = registry.get<MeshComponent>(entity);
                if (!meshComponent.mesh) continue;

                const glm::mat4& worldMatrix = registry.get<Transform>(entity).GetWorldMatrix();

                const auto& positions = meshComponent.mesh->GetCollisionPositions();
                const auto& indices = meshComponent.mesh->GetCollisionIndices();

                for (size_t i = 0; i + 2 < indices.size(); i += 3)
                {
                    glm::vec3 a = glm::vec3(worldMatrix * glm::vec4(positions[indices[i]], 1.0f));
                    glm::vec3 b = glm::vec3(worldMatrix * glm::vec4(positions[indices[i + 1]], 1.0f));
                    glm::vec3 c = glm::vec3(worldMatrix * glm::vec4(positions[indices[i + 2]], 1.0f));

                    float t;
                    if (RayIntersectsTriangle(rayOrigin, rayDir, a, b, c, t) && t < closestT)
                    {
                        closestT = t;
                        closest = entity;
                    }
                }
            }

            return closest;
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

            // Click-to-select, but not when that click is grabbing the gizmo
            if (m_state.viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() &&
                !ImGuizmo::IsUsing())
                TryPickEntity();
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
        engine.GetUndoSystem()->BeginEdit();

        auto entity = ecs->CreateEntity();
        ecs->AddComponent<Guid>(entity, GenerateGuid());
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
        engine.GetUndoSystem()->CommitEdit();

        WD_INFO("Spawned model from asset drop: {}", assetPath);
    }

    void ViewportPanel::TryPickEntity()
    {
        if (m_state.viewportSize.x <= 0.0f || m_state.viewportSize.y <= 0.0f) return;

        auto ecs = engine.GetECS();

        Camera* camera = nullptr;
        for (auto cameraEntity : ecs->View<Camera>())
        {
            auto& cam = ecs->GetComponent<Camera>(cameraEntity);
            if (cam.GetCameraIndex() == 0)
            {
                camera = &cam;
                break;
            }
        }
        if (!camera) return;

        ImVec2 mousePos = ImGui::GetMousePos();
        glm::vec2 localMouse(mousePos.x - m_state.viewportPos.x, mousePos.y - m_state.viewportPos.y);

        glm::vec2 ndc;
        ndc.x = (localMouse.x / m_state.viewportSize.x) * 2.0f - 1.0f;
        ndc.y = 1.0f - (localMouse.y / m_state.viewportSize.y) * 2.0f;

        glm::mat4 invViewProj = glm::inverse(camera->GetProjection() * camera->GetView());

        // GLM_FORCE_DEPTH_ZERO_TO_ONE (see Camera.hpp) means NDC z runs [0,1], not [-1,1]
        glm::vec4 nearPoint = invViewProj * glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
        glm::vec4 farPoint = invViewProj * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        glm::vec3 rayOrigin = glm::vec3(nearPoint);
        glm::vec3 rayDir = glm::normalize(glm::vec3(farPoint) - glm::vec3(nearPoint));

        // Clicking empty space clears the selection, same as the Hierarchy panel does
        m_state.selectedEntity = PickEntityAtRay(ecs->GetRegistry(), rayOrigin, rayDir);
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

        ImGui::SameLine(0.0f, 12.0f);

        auto undoSystem = engine.GetUndoSystem();

        ImGui::BeginDisabled(!undoSystem->CanUndo());
        if (ImGui::Button("Undo")) undoSystem->RequestUndo();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Undo (Ctrl+Z)");
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 4.0f);

        ImGui::BeginDisabled(!undoSystem->CanRedo());
        if (ImGui::Button("Redo")) undoSystem->RequestRedo();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) ImGui::SetTooltip("Redo (Ctrl+Y)");
        ImGui::EndDisabled();

        ImGui::EndGroup();

        if (m_state.viewportHovered && !ImGuizmo::IsUsing() && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_state.gizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_T)) m_state.gizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_Y)) m_state.gizmoOperation = ImGuizmo::SCALE;
        }
    }
} // namespace Wild
