#pragma once

#include "Editor/EditorState.hpp"
#include "Editor/PanelManager.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#include <ImGuizmo.h>
#include <imgui.h>
#include <implot/implot.h>

#include <glm/glm.hpp>

#include <filesystem>
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

        void Prepare();

        // Renders all panels and submits the ImGui draw data to the command list.
        void Draw();

        // Displays a live value in the Debug panel, cheap to call every frame
        template <typename T> void Watch(const std::string& name, T* value);

        void AddPanel(const std::string& name, std::function<void()> renderFunc);

        void DrawGizmo(const glm::mat4& view, const glm::mat4& projection);

        void DrawViewport(Renderer* renderer);

        void DisplayTexture(std::shared_ptr<Texture> texture, glm::vec2 imageSize = {150.0f, 150.0f});
        void DisplayTexture(Texture* texture, glm::vec2 imageSize = {150.0f, 150.0f});

        void SetProfilerDrawCallback(std::function<void()> callback);

        void OnFilesDropped(const char** paths, int count);

        bool IsFullScreen() const { return m_fullscreen; }

        EditorState& GetState() { return m_state; }
        EditorDebugFlags& GetDebugFlags() { return m_state.debug; }
        PanelManager& GetPanels() { return m_panels; }

      private:
        bool Setup(std::shared_ptr<Window> window);
        void RegisterPanels();

        void DrawMenuBar();
        void DrawSaveSceneMenu();
        void DrawOpenSceneMenu();
        void DrawStatsBar();
        void BuildDefaultLayout(ImGuiID dockspaceId);
        void ApplyPendingSceneLoad();
        void HandleCopyPasteShortcuts();

        std::shared_ptr<Window> m_window;

        EditorState m_state;
        PanelManager m_panels;

        // Raw views into panels owned by m_panels
        ViewportPanel* m_viewportPanel = nullptr;
        ProfilerPanel* m_profilerPanel = nullptr;
        AssetBrowserPanel* m_assetBrowserPanel = nullptr;

        std::unordered_map<std::string, std::function<void()>> m_watches;

        bool m_fullscreen = false;
        bool m_rebuildLayout = false;

        char m_scenePathBuffer[256] = "Assets/Scenes/scene.json";

        // Holds a copy of an entity
        std::string m_entityClipboard;

        bool m_wasGizmoUsing = false;

        // Scene loading data
        struct PendingSceneLoad
        {
            enum class Kind
            {
                None,
                NewScene,
                ProceduralScene,
                SceneFile
            } kind = Kind::None;

            std::string proceduralName;
            std::filesystem::path filePath;
        } m_pendingSceneLoad;
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
