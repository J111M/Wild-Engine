#include "Editor/Panels/DebugPanel.hpp"

#include <imgui.h>

namespace Wild
{
    void DebugPanel::OnRender()
    {
        ImGui::SeparatorText("Draw Modes");

        auto& debug = m_state.debug;
        ImGui::Checkbox("Master Debug Draw", &debug.masterDebugDraw);

        ImGui::BeginDisabled(!debug.masterDebugDraw);
        ImGui::Checkbox("Wireframe", &debug.wireframe);
        ImGui::Checkbox("Show Colliders", &debug.showColliders);
        ImGui::Checkbox("Show Normals", &debug.showNormals);
        ImGui::Checkbox("Freeze Culling", &debug.freezeCulling);
        ImGui::EndDisabled();

        // engine.GetImGui()->GetDebugFlags()

        if (!m_watches.empty())
        {
            ImGui::SeparatorText("Watches");
            for (auto& [name, drawValue] : m_watches)
                drawValue();
        }
    }
} // namespace Wild
