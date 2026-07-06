#include "Editor/Panels/InspectorPanel.hpp"

#include "Core/Engine.hpp"
#include "Core/Transform.hpp"
#include "Editor/EditorWidgets.hpp"
#include "Renderer/Passes/PbrPass.hpp"
#include "Renderer/Resources/Mesh.hpp"
#include "Systems/Physics/PhysicsTypes.hpp"

#include <imgui.h>

#include <glm/gtc/quaternion.hpp>

namespace Wild
{
    namespace
    {
        void TrackFieldEdit()
        {
            if (ImGui::IsItemActivated()) engine.GetUndoSystem()->BeginEdit();

            if (ImGui::IsItemDeactivated()) engine.GetUndoSystem()->CommitEdit();
        }

        template <typename Fn> void TrackInstantEdit(Fn&& fn)
        {
            engine.GetUndoSystem()->BeginEdit();
            fn();
            engine.GetUndoSystem()->CommitEdit();
        }

        constexpr const char* kMotionTypeNames[] = {"Static", "Kinematic", "Dynamic"};
        constexpr const char* kColliderTypeNames[] = {"Box", "Sphere", "Capsule", "Convex Hull", "Mesh"};

        void DrawColliderShape(RigidBody& rigidBody, ColliderShape& shape, bool hasMesh)
        {
            int typeIndex = static_cast<int>(shape.type);
            if (ImGui::Combo("Shape Type", &typeIndex, kColliderTypeNames, IM_ARRAYSIZE(kColliderTypeNames)))
            {
                shape.type = static_cast<ColliderType>(typeIndex);

                // Jolt's exact triangle mesh shape only works on Static/Kinematic bodies
                if (shape.type == ColliderType::Mesh && rigidBody.motionType == MotionType::Dynamic)
                    rigidBody.motionType = MotionType::Static;

                rigidBody.bodyDirty = true;
            }
            TrackFieldEdit();

            if ((shape.type == ColliderType::Mesh || shape.type == ColliderType::ConvexHull) && !hasMesh)
                ImGui::TextDisabled("This entity has no mesh - add a MeshComponent for this collider to do anything.");

            switch (shape.type)
            {
            case ColliderType::Box:
                if (EditorWidgets::DragFloat3Colored("Half Extents", shape.boxHalfExtents)) rigidBody.bodyDirty = true;
                TrackFieldEdit();
                break;
            case ColliderType::Sphere:
                if (ImGui::DragFloat("Radius", &shape.sphereRadius, 0.05f, 0.01f, 1000.0f)) rigidBody.bodyDirty = true;
                TrackFieldEdit();
                break;
            case ColliderType::Capsule:
                if (ImGui::DragFloat("Radius", &shape.capsuleRadius, 0.05f, 0.01f, 1000.0f)) rigidBody.bodyDirty = true;
                TrackFieldEdit();
                if (ImGui::DragFloat("Half Height", &shape.capsuleHalfHeight, 0.05f, 0.01f, 1000.0f)) rigidBody.bodyDirty = true;
                TrackFieldEdit();
                break;
            case ColliderType::ConvexHull:
            case ColliderType::Mesh: ImGui::TextDisabled("Uses this entity's mesh geometry"); break;
            }

            if (EditorWidgets::DragFloat3Colored("Offset", shape.offset)) rigidBody.bodyDirty = true;
            TrackFieldEdit();

            glm::vec3 eulerRotation = glm::degrees(glm::eulerAngles(shape.rotation));
            if (EditorWidgets::DragFloat3Colored("Rotation", eulerRotation))
            {
                shape.rotation = glm::quat(glm::radians(eulerRotation));
                rigidBody.bodyDirty = true;
            }
            TrackFieldEdit();

            if (ImGui::SliderFloat("Friction", &shape.friction, 0.0f, 1.0f)) rigidBody.bodyDirty = true;
            TrackFieldEdit();
            if (ImGui::SliderFloat("Restitution", &shape.restitution, 0.0f, 1.0f)) rigidBody.bodyDirty = true;
            TrackFieldEdit();
        }

        void DrawPhysicsBody(EntityComponentSystem& ecs, Entity selected)
        {
            auto& rigidBody = ecs.GetComponent<RigidBody>(selected);
            bool hasMesh = ecs.HasComponent<MeshComponent>(selected);

            int motionTypeIndex = static_cast<int>(rigidBody.motionType);
            if (ImGui::Combo("Motion Type", &motionTypeIndex, kMotionTypeNames, IM_ARRAYSIZE(kMotionTypeNames)))
            {
                rigidBody.motionType = static_cast<MotionType>(motionTypeIndex);
                rigidBody.bodyDirty = true;
            }
            TrackFieldEdit();

            if (ImGui::Checkbox("Use Gravity", &rigidBody.useGravity)) rigidBody.bodyDirty = true;
            TrackFieldEdit();
            ImGui::SameLine();
            if (ImGui::Checkbox("Is Trigger", &rigidBody.isTrigger)) rigidBody.bodyDirty = true;
            TrackFieldEdit();

            if (rigidBody.motionType == MotionType::Dynamic)
            {
                if (ImGui::Checkbox("Auto Mass", &rigidBody.autoMass)) rigidBody.bodyDirty = true;
                TrackFieldEdit();

                if (!rigidBody.autoMass)
                {
                    if (ImGui::DragFloat("Mass", &rigidBody.mass, 0.1f, 0.01f, 100000.0f)) rigidBody.bodyDirty = true;
                    TrackFieldEdit();
                }

                if (ImGui::DragFloat("Linear Damping", &rigidBody.linearDamping, 0.01f, 0.0f, 1.0f)) rigidBody.bodyDirty = true;
                TrackFieldEdit();
                if (ImGui::DragFloat("Angular Damping", &rigidBody.angularDamping, 0.01f, 0.0f, 1.0f)) rigidBody.bodyDirty = true;
                TrackFieldEdit();
            }

            ImGui::Separator();
            ImGui::TextDisabled("Colliders");

            int removeIndex = -1;
            for (size_t i = 0; i < rigidBody.shapes.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));

                std::string label = "Collider " + std::to_string(i) + ": " + kColliderTypeNames[static_cast<int>(rigidBody.shapes[i].type)];
                if (ImGui::TreeNodeEx("ColliderNode", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed, "%s", label.c_str()))
                {
                    DrawColliderShape(rigidBody, rigidBody.shapes[i], hasMesh);

                    if (ImGui::Button("Remove Collider")) removeIndex = static_cast<int>(i);

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            if (removeIndex >= 0)
            {
                TrackInstantEdit([&]() {
                    rigidBody.shapes.erase(rigidBody.shapes.begin() + removeIndex);
                    rigidBody.bodyDirty = true;
                });
            }

            if (ImGui::Button("Add Collider"))
            {
                TrackInstantEdit([&]() {
                    rigidBody.shapes.push_back(ColliderShape{});
                    rigidBody.bodyDirty = true;
                });
            }

            ImGui::Separator();
            if (ImGui::Button("Remove Physics Body"))
            {
                TrackInstantEdit([&]() {
                    engine.GetPhysicsSystem()->DestroyBody(rigidBody);
                    ecs.RemoveComponent<RigidBody>(selected);
                });
            }
        }

        void CollectMeshDescendants(entt::registry& registry, Entity entity, std::vector<Entity>& out)
        {
            auto& transform = registry.get<Transform>(entity);
            for (auto child : transform.GetChildren())
            {
                if (registry.any_of<MeshComponent>(child)) out.push_back(child);
                CollectMeshDescendants(registry, child, out);
            }
        }

        void AddConvexHullCollisionToChildren(EntityComponentSystem& ecs, Entity selected)
        {
            auto& registry = ecs.GetRegistry();

            std::vector<Entity> meshEntities;
            CollectMeshDescendants(registry, selected, meshEntities);
            if (meshEntities.empty()) return;

            TrackInstantEdit([&]() {
                for (auto entity : meshEntities)
                {
                    RigidBody* rigidBody = registry.try_get<RigidBody>(entity);
                    if (!rigidBody) rigidBody = &ecs.AddComponent<RigidBody>(entity);

                    bool hasHullShape = false;
                    for (const auto& shape : rigidBody->shapes)
                    {
                        if (shape.type == ColliderType::ConvexHull)
                        {
                            hasHullShape = true;
                            break;
                        }
                    }

                    if (!hasHullShape)
                    {
                        ColliderShape hullShape{};
                        hullShape.type = ColliderType::ConvexHull;
                        rigidBody->shapes.push_back(hullShape);
                    }

                    rigidBody->bodyDirty = true;
                }
            });
        }

        void DrawAddComponentMenu(EntityComponentSystem& ecs, Entity selected)
        {
            if (ImGui::Button("Add Component")) ImGui::OpenPopup("AddComponentPopup");

            if (ImGui::BeginPopup("AddComponentPopup"))
            {
                bool hasPointLight = ecs.HasComponent<PointLight>(selected);
                bool hasDirectionalLight = ecs.HasComponent<DirectionalLight>(selected);
                bool hasRigidBody = ecs.HasComponent<RigidBody>(selected);

                if (!hasPointLight && ImGui::MenuItem("Point Light"))
                {
                    TrackInstantEdit([&]() { ecs.AddComponent<PointLight>(selected); });
                    ImGui::CloseCurrentPopup();
                }

                if (!hasDirectionalLight && ImGui::MenuItem("Directional Light"))
                {
                    TrackInstantEdit([&]() { ecs.AddComponent<DirectionalLight>(selected); });
                    ImGui::CloseCurrentPopup();
                }

                if (!hasRigidBody && ImGui::MenuItem("Physics Body"))
                {
                    TrackInstantEdit([&]() {
                        auto& rigidBody = ecs.AddComponent<RigidBody>(selected);
                        rigidBody.shapes.push_back(ColliderShape{});
                    });
                    ImGui::CloseCurrentPopup();
                }

                if (hasPointLight && hasDirectionalLight && hasRigidBody) ImGui::TextDisabled("All available components added");

                ImGui::EndPopup();
            }
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

        if (ecs->HasComponent<RigidBody>(selected) && ImGui::CollapsingHeader("Physics Body", ImGuiTreeNodeFlags_DefaultOpen))
            DrawPhysicsBody(*ecs, selected);

        if (ecs->HasComponent<RigidBody>(selected) && registry.get<Transform>(selected).HasChild())
        {
            if (ImGui::Button("Enable Convex Hull Collision on All Children")) AddConvexHullCollisionToChildren(*ecs, selected);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Adds a Physics Body + Convex Hull collider to every descendant with a mesh\n"
                                   "(skips any that already have one). Great for a whole imported model.");
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

        ImGui::Separator();
        DrawAddComponentMenu(*ecs, selected);
    }
} // namespace Wild
