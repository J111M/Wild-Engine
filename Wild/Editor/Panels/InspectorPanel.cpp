#include "Editor/Panels/InspectorPanel.hpp"

#include "Core/Engine.hpp"
#include "Core/Transform.hpp"
#include "Editor/EditorWidgets.hpp"
#include "Renderer/Passes/PbrPass.hpp"
#include "Renderer/Resources/Mesh.hpp"

#include <imgui.h>

#include <glm/gtc/quaternion.hpp>

namespace Wild
{
    namespace
    {
        void TrackFieldEdit()
        {
            if (ImGui::IsItemActivated()) engine.GetUndoSystem()->BeginEdit();
            if (ImGui::IsItemDeactivatedAfterEdit()) engine.GetUndoSystem()->CommitEdit();
        }
    } // namespace

    void InspectorPanel::OnRender()
    {
        auto ecs = engine.GetECS();
        auto& registry = ecs->GetRegistry();

        if (m_state.selectedEntity == entt::null || !registry.valid(m_state.selectedEntity))
        {
            ImGui::TextDisabled("No entity selected");
            return;
        }

        const Entity selected = m_state.selectedEntity;

        if (ecs->HasComponent<Transform>(selected))
        {
            auto& transform = registry.get<Transform>(selected);
            ImGui::Text("%s", transform.Name.empty() ? "Empty Gameobject" : transform.Name.c_str());
        }
        ImGui::TextDisabled("Entity ID: %u", entt::to_integral(selected));
        ImGui::Separator();

        if (ecs->HasComponent<Transform>(selected) && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& transform = registry.get<Transform>(selected);

            if (EditorWidgets::DragFloat3Colored("Position", transform.GetPosition())) transform.MarkDirty();

            glm::vec3 eulerRotation = glm::degrees(glm::eulerAngles(transform.GetRotation()));
            if (EditorWidgets::DragFloat3Colored("Rotation", eulerRotation))
            {
                transform.SetRotation(glm::quat(glm::radians(eulerRotation)));
                transform.MarkDirty();
            }

            if (EditorWidgets::DragFloat3Colored("Scale", transform.GetScale())) transform.MarkDirty();
        }

        if (ecs->HasComponent<MeshComponent>(selected) && ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (auto& mesh = registry.get<MeshComponent>(selected).mesh)
            {
                auto& material = mesh->GetMaterial();

                EditorWidgets::TextureImage(material.m_albedo);
                EditorWidgets::TextureImage(material.m_normal);
                EditorWidgets::TextureImage(material.m_emissive);
                EditorWidgets::TextureImage(material.m_roughnessMetallic);
                EditorWidgets::TextureImage(material.m_occlusion);

                ImGui::Separator();
            }
        }

        if (ecs->HasComponent<PointLight>(selected) && ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& pointLight = registry.get<PointLight>(selected);

            ImGui::ColorEdit3("Color", &pointLight.colorIntensity[0]);
            TrackFieldEdit();
            ImGui::DragFloat("Intensity", &pointLight.colorIntensity.w);
            TrackFieldEdit();
        }

        if (ecs->HasComponent<DirectionalLight>(selected) &&
            ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto& dirLight = registry.get<DirectionalLight>(selected);

            glm::vec3 dir = glm::normalize(glm::vec3(dirLight.direction[0], dirLight.direction[1], dirLight.direction[2]));

            float pitch = glm::degrees(asinf(glm::clamp(dir.y, -1.0f, 1.0f)));
            float yaw = glm::degrees(atan2f(dir.x, dir.z));
            float euler[2] = {pitch, yaw};

            if (ImGui::DragFloat2("Rotation (P/Y)", euler, 0.5f, -180.0f, 180.0f, "%.1f deg"))
            {
                float p = glm::radians(euler[0]);
                float y = glm::radians(euler[1]);

                dirLight.direction[0] = cosf(p) * sinf(y);
                dirLight.direction[1] = sinf(p);
                dirLight.direction[2] = cosf(p) * cosf(y);
            }
            TrackFieldEdit();

            ImGui::ColorEdit3("Color", &dirLight.colorIntensity[0]);
            TrackFieldEdit();
            ImGui::SliderFloat("Intensity", &dirLight.colorIntensity.w, 0.0f, 50.0f);
            TrackFieldEdit();
        }
    }
} // namespace Wild
