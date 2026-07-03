#pragma once

#include "Editor/EditorPanel.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Wild
{
    class PanelManager
    {
      public:
        EditorPanel* Register(std::unique_ptr<EditorPanel> panel);

        void AddCallbackPanel(const std::string& name, const std::string& category, std::function<void()> renderFunc);

        void RenderPanels();
        void RenderWindowMenu();

        EditorPanel* Find(const char* name);

      private:
        std::vector<std::unique_ptr<EditorPanel>> m_panels;
    };
} // namespace Wild
