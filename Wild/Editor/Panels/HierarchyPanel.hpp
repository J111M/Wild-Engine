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
        void DrawSaveAsPrefabPopup();

        EditorState& m_state;
        Entity m_prefabSaveTarget = entt::null;
        bool m_openSaveAsPrefabPopup = false;
#ifdef ASSETS_SOURCE_DIR
        char m_prefabPathBuffer[260] = ASSETS_SOURCE_DIR "/Prefabs/Prefab.prefab";
#else
        char m_prefabPathBuffer[260] = "Assets/Prefabs/Prefab.prefab";
#endif
    };
} // namespace Wild
