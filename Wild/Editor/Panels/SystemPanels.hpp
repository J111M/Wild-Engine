#pragma once

#include "Editor/EditorPanel.hpp"

#include <glm/glm.hpp>

namespace Wild
{
    // WIP
    class OceanSystemPanel final : public EditorPanel
    {
      public:
        OceanSystemPanel() { defaultOpen = false; }

        const char* Name() const override { return "Ocean Rendering"; }
        const char* Category() const override { return "Systems"; }
        void OnRender() override;

      private:
        float m_waveAmplitude = 1.0f;
        glm::vec2 m_windDirection{1.0f, 0.0f};
        float m_choppiness = 0.8f;
        int m_tessellation = 16;
        bool m_foamEnabled = true;
    };

    class VolumetricsPanel final : public EditorPanel
    {
      public:
        VolumetricsPanel() { defaultOpen = false; }

        const char* Name() const override { return "Volumetrics"; }
        const char* Category() const override { return "Systems"; }
        void OnRender() override;

      private:
        float m_density = 0.02f;
        float m_scattering = 0.7f;
        int m_stepCount = 64;
        glm::vec3 m_fogColor{0.5f, 0.6f, 0.7f};
        bool m_godRaysEnabled = true;
    };
} // namespace Wild
