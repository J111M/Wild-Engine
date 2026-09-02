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

    struct DDGIProbeData
    {
        glm::vec4 randomRotation{0.0f, 0.0f, 0.0f, 1.0f}; // quaternion (xyz axis*sin(half), w = cos(half))

        glm::vec4 probeOrigin{};                        // xyz = grid origin, w pad
        glm::vec4 probeSpacing{1.0f, 1.0f, 1.0f, 0.0f}; // xyz = grid spacing, w pad
        glm::ivec4 probeCounts{};                       // xyz = probe grid counts, w = maxRaysPerProbe (buffer stride)

        uint32_t raysPerProbe{64};
        float maxRayDistance{1000.0f};
        float intensity{1.0f};

        float hysteresis{0.97f};

        uint32_t irradianceView{};
        uint32_t distanceView{};
    };

    struct DDGITraceConstants
    {
        glm::mat4 inverseView{1.0f};

        glm::vec4 lightDirection{0.0f, -1.0f, 0.0f, 0.0f};     // xyz = direction the light travels, w unused
        glm::vec4 lightColorIntensity{1.0f, 1.0f, 1.0f, 1.0f}; // rgb = color, a = intensity

        uint32_t environmentView{};

        uint32_t numPointLights{};

        uint32_t probeDataView{INVALID_HEAP_INDEX};

        uint32_t tracePadding{};
    };

    struct DDGIUpdateConstants
    {
        // Raw descriptor heap slots of the ping-pong pairs, resolved through ResourceDescriptorHeap in the shader
        uint32_t irradianceReadView{INVALID_HEAP_INDEX};
        uint32_t irradianceWriteView{INVALID_HEAP_INDEX};

        uint32_t distanceReadView{INVALID_HEAP_INDEX};
        uint32_t distanceWriteView{INVALID_HEAP_INDEX};

        float probeMaxRayDistance{1.0f};

        uint32_t probeDataView{INVALID_HEAP_INDEX};

        glm::uvec2 updatePadding{};
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
        std::shared_ptr<GPUBuffer> m_traceConstantBuffer{};
        std::shared_ptr<GPUBuffer> m_irradianceConstantBuffer{};
        std::shared_ptr<GPUBuffer> m_distanceConstantBuffer{};

        std::shared_ptr<GPUBuffer> m_probeDataBuffer{};

        DDGIProbeData m_probeData{};

        DDGITraceConstants m_traceRc{};
        DDGIUpdateConstants m_updateRc{};

        uint32_t m_frameParity{0};

        void FillUpdateConstants(const DDGIPassData& ddgiData, const ProbeSystem& probeSystem);

        uint32_t GetOrCreateProbeDataView();

        void UploadProbeData(const ProbeSystem& probeSystem, const DDGIPassData& ddgiData);

        static std::shared_ptr<GPUBuffer> CreateConstantBuffer(size_t size, const std::string& name);

        int m_raysPerProbe = 64;
        float m_hysteresis = 0.97f;
        float m_maxRayDistance = 1000.0f;
        float m_intensity = 1.0f;

        bool m_randomRayRotation = true;
        bool m_freezeUpdates = false;
    };
} // namespace Wild
