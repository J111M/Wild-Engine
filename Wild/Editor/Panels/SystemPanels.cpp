#include "Editor/Panels/SystemPanels.hpp"

#include <imgui.h>

namespace Wild
{
    void OceanSystemPanel::OnRender()
    {
        ImGui::SliderFloat("Wave Amplitude", &m_waveAmplitude, 0.0f, 10.0f);
        ImGui::SliderFloat2("Wind Direction", &m_windDirection.x, -1.0f, 1.0f);
        ImGui::SliderFloat("Choppiness", &m_choppiness, 0.0f, 2.0f);
        ImGui::SliderInt("Tessellation", &m_tessellation, 1, 64);
        ImGui::Checkbox("Foam", &m_foamEnabled);
    }

    void VolumetricsPanel::OnRender()
    {
        ImGui::SliderFloat("Density", &m_density, 0.0f, 0.2f, "%.4f");
        ImGui::SliderFloat("Scattering", &m_scattering, 0.0f, 1.0f);
        ImGui::SliderInt("Step Count", &m_stepCount, 8, 256);
        ImGui::ColorEdit3("Fog Color", &m_fogColor.x);
        ImGui::Checkbox("God Rays", &m_godRaysEnabled);
    }
} // namespace Wild
