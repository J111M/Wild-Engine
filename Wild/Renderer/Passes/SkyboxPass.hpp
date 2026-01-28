#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Mesh.hpp"

#include <functional>

namespace Wild
{
    struct SkyPassData
    {
        Texture *finalTexture;
        Texture *depthTexture;
    };

    struct IBLPassData
    {
        Texture *environmentCubeTexture;
        Texture *irradianceTexture;
    };

    struct IrradiancePassData
    {
        Texture *environmentCubeTexture;
        Texture *irradianceTexture;
    };

    struct CameraProjection
    {
        glm::mat4 view{};
        glm::mat4 proj{};
    };

    struct SkyRootConstant
    {
        uint32_t view{};
        uint32_t viewCube{};
        uint32_t mipLevel{};
    };

    struct IBLRootConstant
    {
        glm::mat4 projView{};
        uint32_t view;
    };

    struct SpecularMapRootConstant
    {
        uint32_t environmentView{};
        int mipSize{};
        float roughness{};
        int face{};
    };

    class CommandList;

    class SkyPass : public RenderFeature
    {
      public:
        SkyPass(std::string filePath);
        ~SkyPass() {};

        virtual void Update(const float dt) override;
        virtual void Add(Renderer& renderer, RenderGraph& rg) override;

        void AddSkyboxPass(Renderer& renderer, RenderGraph& rg);
        void AddIBLPass(Renderer& renderer, RenderGraph& rg);

      private:
        // Skybox pass
        std::vector<Vertex> CreateCube();
        std::vector<Vertex> m_cube;

        std::unique_ptr<Buffer> m_cameraProjection[BACK_BUFFER_COUNT];
        std::unique_ptr<Buffer> m_cubeVertexBuffer;
        std::unique_ptr<Texture> m_skyboxTexture;

        uint32_t m_debugSkyboxMode = 0;
        bool ShouldGenerateNewIBL = true;
        // std::function<void(const SkyPassData&, CommandList&)> m_skyboxLambda;

        std::shared_ptr<Texture> m_brdfLut;
        std::shared_ptr<Texture> m_specularMap;
        uint32_t m_specularMips = 5;
    };
} // namespace Wild
