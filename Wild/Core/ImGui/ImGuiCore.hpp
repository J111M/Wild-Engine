#pragma once

#include "Editor/EditorState.hpp"
#include "Editor/PanelManager.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGuizmo.h>
#include <imgui.h>
#include <implot/implot.h>

#include <glm/glm.hpp>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Wild
{
    class Renderer;
    class Texture;
    class Window;
    class ViewportPanel;
    class ProfilerPanel;
    class AssetBrowserPanel;

    class ImguiCore
    {
      public:
        ImguiCore(std::shared_ptr<Window> window);
        ~ImguiCore();

        // New frame + dockspace + menu/status bars. Call before render passes.
        void Prepare();

        // Renders all panels and submits the ImGui draw data to the command list.
        void Draw();

        // Displays a live value in the Debug panel, cheap to call every frame
        template <typename T> void Watch(const std::string& name, T* value);

        // Legacy panel registration used by render passes. Shows up in the
        // Window menu under "Systems". Safe to call every frame.
        void AddPanel(const std::string& name, std::function<void()> renderFunc);

        void DrawGizmo(const glm::mat4& view, const glm::mat4& projection);

        // Feeds the renderer's composite texture into the Viewport panel
        // (or draws it fullscreen when the editor UI is hidden with Ctrl+F)
        void DrawViewport(Renderer* renderer);

        void DisplayTexture(std::shared_ptr<Texture> texture, glm::vec2 imageSize = {150.0f, 150.0f});
        void DisplayTexture(Texture* texture, glm::vec2 imageSize = {150.0f, 150.0f});

        // Hands the Profiler panel its draw content, see ProfilerPanel.hpp
        void SetProfilerDrawCallback(std::function<void()> callback);

        // Target of the GLFW file-drop callback, forwards to the Assets panel
        void OnFilesDropped(const char** paths, int count);

        bool IsFullScreen() const { return m_fullscreen; }

        EditorState& GetState() { return m_state; }
        EditorDebugFlags& GetDebugFlags() { return m_state.debug; }
        PanelManager& GetPanels() { return m_panels; }

      private:
        bool Setup(std::shared_ptr<Window> window);
        void RegisterPanels();

        void DrawMenuBar();
        void DrawStatsBar();
        void BuildDefaultLayout(ImGuiID dockspaceId);

        std::shared_ptr<Window> m_window;

        EditorState m_state;
        PanelManager m_panels;

        // Raw views into panels owned by m_panels
        ViewportPanel* m_viewportPanel = nullptr;
        ProfilerPanel* m_profilerPanel = nullptr;
        AssetBrowserPanel* m_assetBrowserPanel = nullptr;

        std::unordered_map<std::string, std::function<void()>> m_watches;

        bool m_fullscreen = false;
        bool m_rebuildLayout = false; // set on first run and by Window > Reset Layout
    };

    template <typename T> inline void ImguiCore::Watch(const std::string& name, T* value)
    {
        m_watches[name] = [value, name]() {
            if constexpr (std::is_same_v<T, float>) { ImGui::Text("%s: %.2f", name.c_str(), *value); }
            else if constexpr (std::is_same_v<T, int>) { ImGui::Text("%s: %d", name.c_str(), *value); }
            else if constexpr (std::is_same_v<T, uint32_t>) { ImGui::Text("%s: %u", name.c_str(), *value); }
        };
    }
} // namespace Wild
