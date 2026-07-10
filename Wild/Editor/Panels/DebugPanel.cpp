#include "Editor/Panels/DebugPanel.hpp"

#include "Core/Engine.hpp"
#include "Renderer/Passes/DDGIPass.hpp"
#include "Renderer/Passes/ProbeDebugPass.hpp"

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

        ImGui::SeparatorText("Global Illumination");

        auto renderer = engine.GetRenderer();
        if (auto* probePass = renderer ? renderer->GetRenderFeature<ProbeDebugPass>() : nullptr)
        {
            ImGui::Checkbox("Draw GI Probes", &probePass->drawProbes);
        }

        if (auto* ddgiPass = renderer ? renderer->GetRenderFeature<DDGIPass>() : nullptr)
        {
            ImGui::Checkbox("Enable DDGI", &ddgiPass->enabled);
        }

        // engine.GetImGui()->GetDebugFlags()

        if (!m_watches.empty())
        {
            ImGui::SeparatorText("Watches");
            for (auto& [name, drawValue] : m_watches)
                drawValue();
        }
    }
} // namespace Wild
