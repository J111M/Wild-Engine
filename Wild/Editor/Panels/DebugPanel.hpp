#pragma once

#include "Editor/EditorPanel.hpp"
#include "Editor/EditorState.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace Wild
{
    class DebugPanel final : public EditorPanel
    {
      public:
        DebugPanel(EditorState& state, std::unordered_map<std::string, std::function<void()>>& watches)
            : m_state(state), m_watches(watches)
        {
            defaultOpen = false;
        }

        const char* Name() const override { return "Debug"; }
        const char* Category() const override { return "Debug"; }
        void OnRender() override;

      private:
        EditorState& m_state;
        std::unordered_map<std::string, std::function<void()>>& m_watches;
    };
} // namespace Wild
