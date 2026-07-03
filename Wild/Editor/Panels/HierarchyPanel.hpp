#pragma once

#include "Editor/EditorPanel.hpp"
#include "Editor/EditorState.hpp"

#include <entt/entt.hpp>

namespace Wild
{
    class HierarchyPanel final : public EditorPanel
    {
      public:
        explicit HierarchyPanel(EditorState& state) : m_state(state) {}

        const char* Name() const override { return "Hierarchy"; }
        const char* Category() const override { return "Core"; }
        void OnRender() override;

      private:
        void DrawEntityNode(entt::registry& registry, Entity entity);

        EditorState& m_state;
    };
} // namespace Wild
