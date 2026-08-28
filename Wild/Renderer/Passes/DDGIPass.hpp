#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Systems/ProbeSystem.hpp"

namespace Wild
{
    class GPUBuffer;

    // No descriptor is ever allocated at this index, the heap hands out slots starting at 1
    static constexpr uint32_t INVALID_HEAP_INDEX = UINT32_MAX;

    // Shared by every DDGI pass, mirrors the DDGIConstants cbuffer in Shaders/DDGI/DDGIConstants.slang.
    // Both files must stay in sync, the trace and the update passes bind this same struct to b0.
    struct DDGIRootConstants
    {
        glm::mat4 inverseView{1.0f};

        glm::vec4 randomRotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion (xyz axis*sin(half), w = cos(half))

        glm::vec4 lightDirection{0.0f, -1.0f, 0.0f, 0.0f};     // xyz = direction the light travels, w unused
        glm::vec4 lightColorIntensity{1.0f, 1.0f, 1.0f, 1.0f}; // rgb = color, a = intensity

        glm::vec4 probeOrigin{};                        // xyz = grid origin, w unused
        glm::vec4 probeSpacing{1.0f, 1.0f, 1.0f, 0.0f}; // xyz = grid spacing, w unused
        glm::ivec4 probeCounts{};                       // xyz = probe grid counts, w = maxRaysPerProbe (buffer stride)

        uint32_t raysPerProbe{64};
        float maxRayDistance{1000.0f};
        float intensity{1.0f};
        uint32_t environmentView{};

        uint32_t numPointLights{};
        uint32_t irradianceView{};
        uint32_t distanceView{};
        float hysteresis{0.97f};

        // Descriptor heap slots of the ping-pong irradiance pair, raw heap indices for ResourceDescriptorHeap.
        // INVALID_HEAP_INDEX means the view could not be resolved and the update pass must skip its dispatch,
        // heap slot 0 is never handed out so it is not usable as a "none" value.
        uint32_t irradianceReadView{INVALID_HEAP_INDEX};
        uint32_t irradianceWriteView{INVALID_HEAP_INDEX};
        uint32_t ddgiPadding[2]{};
    };

    // The cbuffer is 13 16-byte rows
    static_assert(sizeof(DDGIRootConstants) == 208, "DDGIRootConstants must match the DDGIConstants cbuffer layout");

    // Double buffering so that the read and writing is done in different frames
    struct DDGIPassData
    {
        // Take care of which buffer to use
        uint32_t frameParity = 0;

        Texture* iradianceTexture[BACK_BUFFER_COUNT];
        Texture* visibilityTexture[BACK_BUFFER_COUNT];
    };

    struct UpdateIrradiancePassData
    {
    };

    struct UpdateDistancePassData
    {
    };

    class DDGIPass : public RenderFeature
    {
      public:
        DDGIPass();
        ~DDGIPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

        void AddProbeTracePass(Renderer& renderer, RenderGraph& rg);
        void AddUpdateIrradiancePass(Renderer& renderer, RenderGraph& rg);
        void AddUpdateDistancePass(Renderer& renderer, RenderGraph& rg);

        bool enabled = false;

      private:
        // Backs the DDGIConstants cbuffer for all three passes, uploaded once per frame by the trace pass
        std::shared_ptr<GPUBuffer> m_ddgiConstantBuffer{};

        DDGIRootConstants m_ddgiRc{};

        int m_raysPerProbe = 64;
        float m_hysteresis = 0.97f;
        float m_maxRayDistance = 1000.0f;
        float m_intensity = 1.0f;

        bool m_randomRayRotation = true;
        bool m_freezeUpdates = false;
    };
} // namespace Wild
