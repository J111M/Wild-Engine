#include "Editor/Panels/HierarchyPanel.hpp"

#include "Core/Engine.hpp"
#include "Core/Transform.hpp"

#include <ImGuizmo.h>
#include <imgui.h>

namespace Wild
{
    void HierarchyPanel::OnRender()
    {
        auto& registry = engine.GetECS()->GetRegistry();

        // Clicking empty space in the panel clears the selection
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() &&
            !ImGuizmo::IsOver())
        {
            m_state.selectedEntity = entt::null;
        }

        float rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        float windowWidth = contentSize.x;
        float windowHeight = contentSize.y;

        ImVec2 windowStartPos = ImGui::GetCursorScreenPos();

        ImU32 rowEven = ImGui::GetColorU32(ImGuiCol_TableRowBg);    // window bg shows through
        ImU32 rowOdd = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt);  // subtle lift over the bg

        int numRows = static_cast<int>(windowHeight / rowHeight);
        for (int i = 0; i < numRows; i++)
        {
            ImVec2 rowPos = ImVec2(windowStartPos.x, windowStartPos.y + i * rowHeight);

            ImU32 bgColor = (i % 2 == 0) ? rowEven : rowOdd;

            drawList->AddRectFilled(rowPos, ImVec2(rowPos.x + windowWidth, rowPos.y + rowHeight), bgColor);
        }

        registry.view<Transform>().each([&](auto entity, Transform& transform) {
            if (!transform.HasParent()) DrawEntityNode(registry, entity);
        });
    }

    void HierarchyPanel::DrawEntityNode(entt::registry& registry, Entity entity)
    {
        if (!registry.valid(entity)) return;

        auto& transform = registry.get<Transform>(entity);

        std::string label = transform.Name.empty() ? "Empty Gameobject" : transform.Name;

        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (transform.GetChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
        if (m_state.selectedEntity == entity) flags |= ImGuiTreeNodeFlags_Selected;

        bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", label.c_str());

        if (ImGui::IsItemClicked()) m_state.selectedEntity = entity;

        if (nodeOpen)
        {
            for (auto child : transform.GetChildren())
                DrawEntityNode(registry, child);
            ImGui::TreePop();
        }
    }
} // namespace Wild
