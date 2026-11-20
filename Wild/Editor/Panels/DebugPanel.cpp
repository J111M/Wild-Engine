#include "Editor/Panels/DebugPanel.hpp"

#include "Core/Engine.hpp"
#include "Renderer/Passes/DDGIPass.hpp"
#include "Renderer/Passes/ProbeDebugPass.hpp"
#include "Systems/ProbeSystem.hpp"

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
        ImGui::EndDisabled();

        ImGui::SeparatorText("Global Illumination");

        auto renderer = engine.GetRenderer();
        if (auto* probePass = renderer ? renderer->GetRenderFeature<ProbeDebugPass>() : nullptr)
        {
            ImGui::Checkbox("Draw GI Probes", &probePass->drawProbes);

            ImGui::BeginDisabled(!probePass->drawProbes);
            ImGui::SliderFloat(
                "Probe Exposure", &probePass->irradianceExposure, 0.1f, 500.0f, "%.1f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("Probe Size", &probePass->probeScale, 0.01f, 0.25f);

            const int maxCascade = static_cast<int>(ProbeSystem::MAX_CASCADES) - 1;
            ImGui::SliderInt("Probe Cascade", &probePass->debugCascade, -1, maxCascade, probePass->debugCascade < 0 ? "All" : "%d");
            ImGui::EndDisabled();
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
