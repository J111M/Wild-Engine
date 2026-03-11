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
        float scale{1.0f};
        float angle{0.3f};
        float spreadBlend{1.0f};
        float swell{0.198f};

        float alpha{0.0081f};   // Phillips constant, classic JONSWAP default
        float peakOmega{1.56f}; // peak frequency ~2 rad/s is typical for moderate wind
        float gamma{4.3f};      // JONSWAP peak enhancement factor, standard default
        float shortWavesFade{0.5f};
    };

    struct InitialSpectrumRC
    {
        SpectrumSettings m_spectrumSettings[2];

        uint32_t lengthScale[4] = {250, 32, 16, 4};

        float oceanDepth = 200.0f;

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
        uint32_t fourierTextureView1{};
    };

    /// <summary>
    /// Ocean render pass
    /// </summary>

    struct OceanPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
    };

    struct OceanRenderRC
    {
        glm::mat4 worldMatix{};
        glm::mat4 invModel{};
        uint32_t displacementMapView{};
        uint32_t slopeMapView{};
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

        float m_fetch[2] = {800000.0f, 100000.0f};
        float m_windSpeed[2] = {12.0f, 8.0f};

        /// Update spectrum
        UpdateSpectrumRC m_updateSpectrumRC{};

        /// FFT
        IFFTRC m_ifftRC;

        ///  Draw ocean
        OceanRenderRC m_oceanRc{};

        Entity m_chunkEntity;

        // Ocean mesh
        std::unique_ptr<Buffer> m_oceanVertices;
        std::unique_ptr<Buffer> m_oceanIndices;
        uint32_t m_drawCount{};
    };

} // namespace Wild
