#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
#define OCEAN_SIZE 512
#define OCEAN_CASCADES 4 // Max cascades of 4
#define GRAVITY 9.81

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
        float scale{9.0f};
        float angle{2.9f};
        float spreadBlend{0.7f};
        float swell{0.198f};

        float alpha{0.4f};
        float peakOmega{1.56f};
        float gamma{4.3f};
        float shortWavesFade{0.2f};
    };

    struct InitialSpectrumRC
    {
        SpectrumSettings m_spectrumSettings[2];

        uint32_t lengthScale[4] = {250, 32, 16, 4};

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
        uint32_t lengthScale[4] = {250, 32, 16, 4};

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
    /// Ocean render pass
    /// </summary>

    struct OceanCameraData
    {
        glm::mat4 projViewMatrix{};
        glm::mat4 modelMatix{};
        glm::mat4 invModel{};
    };

    struct OceanPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
    };

    struct OceanRenderRC
    {
        glm::vec4 cameraPosition{};

        float uvScalars[4] = {2.9f, 5.7f, 8.2f, 10.0f};

        uint32_t displacementMapView{};
        uint32_t slopeMapView{};
        uint32_t irradianceView{};
        uint32_t specularView{};

        float normalScalar = 30.0f;
        float reflectionScalar = 0.9;
        float waveHeightScalar = 1.0;
        float airBubbleDensity = 0.3;

        glm::vec4 waterScatterColor = glm::vec4(0.0, 0.08, 0.13, 1);
        glm::vec4 airBubblesColor = glm::vec4(0.0, 0.41, 0.41, 1);
        glm::vec4 foamColor = glm::vec4(1, 1, 1, 1);

        float peakScatterStrength = 1.5;
        float scatterStrength = 1.5;
        float scatterShadowStrength = 1.5;
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
        void AddDrawOceanPass(Renderer& renderer, RenderGraph& rg);

        void GenerateOceanPlane(uint32_t resolution = 128);

        // Box muller formula for generating gaussian distrubted numbers
        std::pair<float, float> BoxMuller(float u1, float u2);

        /// Initial spectrum
        bool m_recomputeInitialSpectrum = true;
        std::unique_ptr<Buffer> m_gaussianDistribution{};
        InitialSpectrumRC m_initialSpectrumRC{};

        float m_fetch[2] = {112299.0f, 100000.0f};
        float m_windSpeed[2] = {12.0f, 8.0f};

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
        std::unique_ptr<Buffer> m_oceanVertices;
        std::unique_ptr<Buffer> m_oceanIndices;
        std::unique_ptr<Buffer> m_cameraBuffer{};
        uint32_t m_drawCount{};
    };

} // namespace Wild
