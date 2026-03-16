#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"
#include "Systems/OceanChunkSystem.hpp"

namespace Wild
{
#define OCEAN_SIZE 512
#define OCEAN_CASCADES 4 // Max cascades of 4
#define GRAVITY 9.81
#define MAX_OCEAN_LOD 3

    // Gaussian noise
    struct ComplexValue
    {
        float real, imaginary;
    };

    /// <summary>
    /// Initial spectrum pass
    /// </summary>

    struct InitialSpectrumPassData
    {
        Texture* initialSpectrumTexture;
    };

    struct SpectrumSettings
    {
        float scale{26.0f};
        float angle{2.9f};
        float spreadBlend{1.0f};
        float swell{0.8f};

        float alpha{0.4f};
        float peakOmega{2.1f};
        float gamma{3.37f};
        float shortWavesFade{0.0f};
    };

    struct InitialSpectrumRC
    {
        SpectrumSettings m_spectrumSettings[2];

        float lengthScale[4] = {250, 32, 16, 4};

        float oceanDepth = 60.0f;

        uint32_t oceanSize = OCEAN_SIZE;
    };

    /// <summary>
    /// Conjugate pass
    /// </summary>

    struct ConjugatePassData
    {
        Texture* conjugatedTexture;
    };

    /// <summary>
    /// Update spectrum pass
    /// </summary>

    struct UpdateSpectrumPassData
    {
        Texture* spectrumTexture;
    };

    struct UpdateSpectrumRC
    {
        float lengthScale[4] = {250, 32, 16, 4};

        float time{};
        uint32_t oceanSize = OCEAN_SIZE;
        uint32_t h0TextureView{};
    };

    /// <summary>
    /// Fast fourier transform
    /// <summary>

    struct IFFTPassData
    {
        Texture* fourierTarget;
    };

    struct IFFTRC
    {
        uint32_t oceanSize = OCEAN_SIZE;
        uint32_t axisFlag{};
        uint32_t spectrumTextureView{};
    };

    /// <summary>
    /// Assemble ocean render pass
    /// </summary>

    struct AssembleOceanPassData
    {
        Texture* displacementTexture;
        Texture* slopeTexture;
    };

    struct AssembleOceanRC
    {
        uint32_t oceanSize = OCEAN_SIZE;
        uint32_t fourierTextureView{};

        float foamBias = 1.5f;
        float foamDecayRate = 2.5f;
        float foamThreshold = 0.68f; // Set between 50 - 55
        float foamAdd = 0.5f;
    };

    /// <summary>
    /// Foam filter render pass
    /// <summary>

    struct FoamFilterPassData
    {
        Texture* displacementTexture;
        Texture* slopeTexture;
    };

    /// <summary>
    /// Ocean render pass
    /// </summary>

    struct OceanCameraData
    {
        glm::mat4 projViewMatrix{};
        glm::mat4 invModel{};
    };

    struct OceanPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
    };

    struct OceanRenderData
    {
        float uvScalars[4] = {1.0f / 250.0f, 1.0f / 32.0f, 1.0f / 16.0f, 1.0f / 4.0f};

        float normalScalar = 30.0f;
        float reflectionScalar = 0.9;
        float waveHeightScalar = 1.0;
        float airBubbleDensity = 0.3;

        glm::vec4 waterScatterColor = glm::vec4(0.0, 0.08, 0.13, 1);
        glm::vec4 airBubblesColor = glm::vec4(0.004, 0.40, 0.36, 1);
        glm::vec4 foamColor = glm::vec4(1, 1, 1, 1);

        float peakScatterStrength = 1.5;
        float scatterStrength = 1.5;
        float scatterShadowStrength = 1.5;
        float pad;
    };

    struct OceanRenderRC
    {
        glm::mat4 modelMatrix{};
        glm::vec4 cameraPosition{};
        glm::vec4 lightDirectionIntensity{};

        uint32_t displacementMapView{};
        uint32_t slopeMapView{};
        uint32_t irradianceView{};
        uint32_t specularView{};
    };

    class OceanPass : public RenderFeature
    {
      public:
        OceanPass();
        ~OceanPass() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        void CalculateIntitialSpectrum(Renderer& renderer, RenderGraph& rg);
        void ConjugateSpectrumPass(Renderer& renderer, RenderGraph& rg);
        void UpdateSpectrum(Renderer& renderer, RenderGraph& rg);
        void FastFourierPass(Renderer& renderer, RenderGraph& rg);
        void AssembleOceanPass(Renderer& renderer, RenderGraph& rg);
        void FoamFilterPass(Renderer& renderer, RenderGraph& rg);

        void AddDrawOceanPass(Renderer& renderer, RenderGraph& rg);

        void GenerateOceanPlane(uint32_t resolution = 128);

        std::unique_ptr<OceanChunkSystem> m_oceanChunkSystem;
        bool m_frustumCullingEnabled = true;

        // Box muller formula for generating gaussian distrubted numbers
        std::pair<float, float> BoxMuller(float u1, float u2);

        /// Initial spectrum
        bool m_recomputeInitialSpectrum = true;
        std::unique_ptr<Buffer> m_gaussianDistribution{};
        InitialSpectrumRC m_initialSpectrumRC{};

        float m_fetch[2] = {112299.0f, 100000.0f};
        float m_windSpeed[2] = {12.0f, 8.0f};
        float m_lenghtScales[4] = {810.6f, 597.2f, 378.5f, 236.2f};

        /// Update spectrum
        UpdateSpectrumRC m_updateSpectrumRC{};

        /// FFT
        IFFTRC m_ifftRC;

        ///  Texture assemble data
        AssembleOceanRC m_assembleRC{};

        ///  Draw ocean
        OceanRenderRC m_oceanRC{};

        Entity m_chunkEntity;

        // Ocean mesh
        std::unique_ptr<Buffer> m_oceanVertices[MAX_OCEAN_LOD];
        std::unique_ptr<Buffer> m_oceanIndices[MAX_OCEAN_LOD];
        uint32_t m_drawCount[MAX_OCEAN_LOD]{};

        OceanRenderData m_oceanRenderData{};
        std::unique_ptr<Buffer> m_oceanRenderDataBuffer;

        std::unique_ptr<Buffer> m_cameraBuffer{};
    };

} // namespace Wild
