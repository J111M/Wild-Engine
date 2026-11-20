#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Systems/ProbeSystem.hpp"

namespace Wild
{
    class GPUBuffer;

    struct DDGICascadeData
    {
        glm::ivec4 originCell{}; // w = ray buffer slot or -1
        glm::ivec4 newOriginCell{};
        glm::vec4 spacing{}; // x = spacing, y = distance update max ray distance
    };
    static_assert(sizeof(DDGICascadeData) == 48);

    struct DDGIProbeData
    {
        glm::vec4 randomRotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion (xyz axis*sin(half), w = cos(half))

        glm::ivec4 probeCounts{}; // w = max rays per probe

        uint32_t raysPerProbe{64};
        float maxRayDistance{1000.0f};
        float intensity{1.0f};
        float hysteresis{0.97f};

        uint32_t irradianceView{};
        uint32_t distanceView{};
        float normalBias{0.1f};
        float viewBias{0.1f};

        uint32_t cascadeCount{};
        uint32_t tracedCascadeCount{};
        float cascadeBlendCells{2.0f};
        uint32_t probeDataPadding{};

        glm::uvec4 tracedCascades{};

        DDGICascadeData cascades[ProbeSystem::MAX_CASCADES]{};
    };
    static_assert(sizeof(DDGIProbeData) == 96 + 48 * ProbeSystem::MAX_CASCADES);

    struct DDGITraceConstants
    {
        glm::vec4 lightDirection{0.0f, -1.0f, 0.0f, 0.0f};     // xyz = direction the light travels, w unused
        glm::vec4 lightColorIntensity{1.0f, 1.0f, 1.0f, 1.0f}; // rgb = color, a = intensity

        uint32_t environmentView{};

        uint32_t numPointLights{};

        uint32_t probeDataView{INVALID_HEAP_INDEX};

        float emissiveIntensity{1.0f};
    };

    struct DDGIUpdateConstants
    {
        // Raw descriptor heap slots of the ping-pong pairs, resolved through ResourceDescriptorHeap in the shader
        uint32_t irradianceReadView{INVALID_HEAP_INDEX};
        uint32_t irradianceWriteView{INVALID_HEAP_INDEX};

        uint32_t distanceReadView{INVALID_HEAP_INDEX};
        uint32_t distanceWriteView{INVALID_HEAP_INDEX};

        uint32_t probeDataView{INVALID_HEAP_INDEX};

        glm::uvec3 updatePadding{};
    };

    // Double buffering so that the read and writing is done in different frames
    struct DDGIPassData
    {
        // Decides which pingpong buffer part to use
        uint32_t frameParity = 0;

        uint32_t probeDataView = INVALID_HEAP_INDEX;

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
        // One copy per frame in flight
        PerFrame<std::unique_ptr<GPUBuffer>> m_traceConstantBuffers;
        PerFrame<std::unique_ptr<GPUBuffer>> m_irradianceConstantBuffers;
        PerFrame<std::unique_ptr<GPUBuffer>> m_distanceConstantBuffers;
        PerFrame<std::unique_ptr<GPUBuffer>> m_probeDataBuffers;

        DDGIProbeData m_probeData{};

        DDGITraceConstants m_traceRc{};
        DDGIUpdateConstants m_updateRc{};

        uint32_t m_frameParity{0};

        void FillUpdateConstants(const DDGIPassData& ddgiData, const ProbeSystem& probeSystem);

        uint32_t GetOrCreateProbeDataView();

        void UploadProbeData(const ProbeSystem& probeSystem, const DDGIPassData& ddgiData);

        static std::unique_ptr<GPUBuffer> CreateConstantBuffer(size_t size, const std::string& name);

        int m_raysPerProbe = 64;
        float m_hysteresis = 0.97f;
        float m_maxRayDistance = 1000.0f;
        float m_intensity = 1.0f;
        float m_normalBias = 0.1f;
        float m_viewBias = 0.1f;
        float m_emissiveIntensity = 1.0f;
        float m_cascadeBlendCells = 2.0f;

        bool m_randomRayRotation = true;
        bool m_freezeUpdates = false;
    };
} // namespace Wild
