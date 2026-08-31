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

    // Root data for the DDGI probe trace pass, mirrors the DDGITraceConstants cbuffer in
    // Shaders/DDGI/DDGITraceConstants.slang. Both files must stay in sync.
    struct DDGITraceConstants
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
        // Raw descriptor heap slots, the trace shader resolves the probe atlases through DescriptorHandle
        // because a Texture2DArray cannot be indexed through the bindless Texture2D descriptor table range
        uint32_t irradianceView{};
        uint32_t distanceView{};
        uint32_t tracePadding{};
    };

    // The cbuffer is 12, 16-byte rows
    static_assert(sizeof(DDGITraceConstants) == 192, "DDGITraceConstants must match the DDGITraceConstants cbuffer layout");

    // Root data shared by the two update passes, irradiance and distance. They run the same gather over the ray
    // data buffer and only differ in which ping-pong pair they resolve, so one struct covers both. Mirrors the
    // DDGIUpdateConstants cbuffer in Shaders/DDGI/DDGIUpdateConstants.slang, both files must stay in sync.
    // randomRotation, probeCounts and raysPerProbe are written from the same CPU values as the trace pass, so the
    // ray directions the update passes rebuild are the ones that were traced.
    struct DDGIUpdateConstants
    {
        glm::vec4 randomRotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion (xyz axis*sin(half), w = cos(half))

        glm::ivec4 probeCounts{}; // xyz = probe grid counts, w = maxRaysPerProbe (buffer stride)

        uint32_t raysPerProbe{64};
        float hysteresis{0.97f};

        // Raw descriptor heap slots of the ping-pong pairs, resolved through ResourceDescriptorHeap in the shader
        uint32_t irradianceReadView{INVALID_HEAP_INDEX};
        uint32_t irradianceWriteView{INVALID_HEAP_INDEX};

        uint32_t distanceReadView{INVALID_HEAP_INDEX};
        uint32_t distanceWriteView{INVALID_HEAP_INDEX};

        // Ceiling on the hit distance the distance pass stores, derived from the probe spacing the way RTXGI
        // does it rather than from maxRayDistance, since the second moment is a distance squared
        float probeMaxRayDistance{1.0f};
        uint32_t updatePadding{};
    };

    // The cbuffer is 4, 16-byte rows
    static_assert(sizeof(DDGIUpdateConstants) == 64, "DDGIUpdateConstants must match the DDGIUpdateConstants cbuffer layout");

    // Double buffering so that the read and writing is done in different frames
    struct DDGIPassData
    {
        // Which half of the ping-pong pair to read this frame, seeded from DDGIPass::m_frameParity at graph build
        // time. It cannot be advanced from here, see the comment on that member.
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

        bool enabled = true;

      private:
        // One constant buffer per pass. SetConstantBufferView binds a raw GPU address, so a buffer shared between
        // passes would only ever show the GPU the last CPU write, even though the two update passes record into
        // the same command list. They share the struct, not the storage.
        std::shared_ptr<GPUBuffer> m_traceConstantBuffer{};
        std::shared_ptr<GPUBuffer> m_irradianceConstantBuffer{};
        std::shared_ptr<GPUBuffer> m_distanceConstantBuffer{};

        DDGITraceConstants m_traceRc{};
        DDGIUpdateConstants m_updateRc{};

        uint32_t m_frameParity{0};

        void FillUpdateConstants(const DDGIPassData& ddgiData, const ProbeSystem& probeSystem);

        static std::shared_ptr<GPUBuffer> CreateConstantBuffer(size_t size, const std::string& name);

        int m_raysPerProbe = 64;
        float m_hysteresis = 0.97f;
        float m_maxRayDistance = 1000.0f;
        float m_intensity = 1.0f;

        bool m_randomRayRotation = true;
        bool m_freezeUpdates = false;
    };
} // namespace Wild
