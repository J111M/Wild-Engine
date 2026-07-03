#include "Editor/PanelManager.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

namespace Wild
{
    namespace
    {
        class CallbackPanel final : public EditorPanel
        {
          public:
            CallbackPanel(std::string name, std::string category, std::function<void()> renderFunc)
                : m_name(std::move(name)), m_category(std::move(category)), m_renderFunc(std::move(renderFunc))
            {
            }

            const char* Name() const override { return m_name.c_str(); }
            const char* Category() const override { return m_category.c_str(); }
            void OnRender() override
            {
                if (m_renderFunc) m_renderFunc();
            }

            void SetCallback(std::function<void()> renderFunc) { m_renderFunc = std::move(renderFunc); }

          private:
            std::string m_name;
            std::string m_category;
            std::function<void()> m_renderFunc;
        };
    } // namespace

    EditorPanel* PanelManager::Register(std::unique_ptr<EditorPanel> panel)
    {
        panel->visible = panel->defaultOpen;
        m_panels.push_back(std::move(panel));
        return m_panels.back().get();
    }

    void PanelManager::AddCallbackPanel(const std::string& name, const std::string& category, std::function<void()> renderFunc)
    {
        if (auto* existing = Find(name.c_str()))
        {
            static_cast<CallbackPanel*>(existing)->SetCallback(std::move(renderFunc));
            return;
        }
        Register(std::make_unique<CallbackPanel>(name, category, std::move(renderFunc)));
    }

    void PanelManager::RenderPanels()
    {
        for (auto& panel : m_panels)
        {
            if (!panel->visible) continue;

            if (panel->NoPadding()) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

            bool open = ImGui::Begin(panel->Name(), &panel->visible, panel->WindowFlags());

            if (panel->NoPadding()) ImGui::PopStyleVar();

            if (open) panel->OnRender();
            ImGui::End();
        }
    }

    void PanelManager::RenderWindowMenu()
    {
        std::vector<std::string> categories = {"Core", "Systems", "Debug"};
        for (auto& panel : m_panels)
        {
            const std::string category = panel->Category();
            if (std::find(categories.begin(), categories.end(), category) == categories.end()) categories.push_back(category);
        }

        for (const auto& category : categories)
        {
            bool hasAny = false;
            for (auto& panel : m_panels)
            {
                if (category != panel->Category()) continue;
                if (!hasAny)
                {
                    ImGui::SeparatorText(category.c_str());
                    hasAny = true;
                }
                ImGui::MenuItem(panel->Name(), nullptr, &panel->visible);
            }
        }
    }

    EditorPanel* PanelManager::Find(const char* name)
    {
        for (auto& panel : m_panels)
        {
            if (std::strcmp(panel->Name(), name) == 0) return panel.get();
        }
        return nullptr;
    }
} // namespace Wild
