#pragma once

#include "Editor/EditorPanel.hpp"
#include "Editor/EditorState.hpp"

namespace Wild
{
    class InspectorPanel final : public EditorPanel
    {
      public:
        explicit InspectorPanel(EditorState& state) : m_state(state) {}

        const char* Name() const override { return "Inspector"; }
        const char* Category() const override { return "Core"; }
        void OnRender() override;

      private:
        EditorState& m_state;
    };
} // namespace Wild
