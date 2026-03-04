#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_glfw.h"
#include <ImGuizmo.h>
#include <imgui.h>
#include <implot/implot.h>

namespace Wild
{
    class ImguiCore
    {
      public:
        ImguiCore(std::shared_ptr<Window> window);
        ~ImguiCore();

        void Prepare();
        void Draw();

        template <typename T> void Watch(const std::string& name, T* value);

        void AddPanel(const std::string& name, std::function<void()> renderFunc);

        void DrawGizmo(const glm::mat4& view, const glm::mat4& projection);
        void DrawViewport(Renderer* renderer);

        bool SeperateDragFloat3(const char* name, glm::vec3& value);
        void DisplayTexture(std::shared_ptr<Texture> texture);
        void DisplayTexture(Texture* texture);

      private:
        bool Setup(std::shared_ptr<Window> window);

        void DrawMenuBar();
        void DrawInspectorWindow();
        void AddStatsBar();
        void DrawEntityNode(entt::registry& registry, entt::entity entity);
        void ConvertToImGuizmoMatrix(const glm::mat4& glmMatrix, float* guizmoMatrix);

        entt::entity m_selectedEntity = entt::null;

        ImGuizmo::OPERATION m_gizmoOperation = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE m_gizmoMode = ImGuizmo::WORLD;

        std::unordered_map<std::string, std::function<void()>> m_panels;
        // All visible panels
        std::unordered_map<std::string, bool> m_panelVisible;

        std::unordered_map<std::string, std::function<void()>> m_watches;
    };

    template <typename T> inline void ImguiCore::Watch(const std::string& name, T* value)
    {
        m_watches[name] = [value, name]() {
            if constexpr (std::is_same_v<T, float>) { ImGui::Text("%s: %.2f", name.c_str(), *value); }
            else if constexpr (std::is_same_v<T, int>) { ImGui::Text("%s: %d", name.c_str(), *value); }
            else if constexpr (std::is_same_v<T, uint32_t>) { ImGui::Text("%s: %d", name.c_str(), *value); }
        };
    }
} // namespace Wild
