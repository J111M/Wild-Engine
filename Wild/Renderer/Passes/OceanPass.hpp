#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
#define OCEAN_SIZE 512

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

    struct InitialSpectrumRC
    {
        float scale{0.3f};
        float angle{0.0f};       // wind direction in radians
        float spreadBlend{1.0f}; // fully spread
        float swell{0.0f};       // no swell

        float alpha{0.0081f};   // Phillips constant, classic JONSWAP default
        float peakOmega{1.56f}; // peak frequency ~2 rad/s is typical for moderate wind
        float gamma{3.3f};      // JONSWAP peak enhancement factor, standard default
        float shortWavesFade{0.1f};

        float oceanDepth = 20.0f;

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
        Texture* spectrumTexture1;
    };

    struct UpdateSpectrumRC
    {
        float time{};
        uint32_t oceanSize = OCEAN_SIZE;
        uint32_t h0TextureView{};
    };

    /// <summary>
    /// Fast fourier transform
    /// <summary>

    struct IFFTPassData
    {
        Texture* displacementMap;
        Texture* slopeMap;
    };

    struct IFFTRC
    {
        uint32_t oceanSize = OCEAN_SIZE;
        uint32_t horizontalFlag{};
        uint32_t spectrumTextureView{};
        uint32_t spectrumTextureView2{};
    };

    /// <summary>
    /// Ocean render pass
    /// </summary>
    ///
    struct OceanPassData
    {
        Texture* finalTexture;
        Texture* depthTexture;
    };

    struct OceanRenderRC
    {
        glm::mat4 worldMatix{};
        glm::mat4 invModel{};
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
        void AddDrawOceanPass(Renderer& renderer, RenderGraph& rg);

        void GenerateOceanPlane(uint32_t resolution = 128);

        // Box muller formula for generating gaussian distrubted numbers
        std::pair<float, float> BoxMuller(float u1, float u2);

        /// Initial spectrum
        bool m_recomputeInitialSpectrum = true;
        std::unique_ptr<Buffer> m_gaussianDistribution{};
        InitialSpectrumRC m_initialSpectrumRC{};

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
