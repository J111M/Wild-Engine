#pragma once
#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

namespace Wild
{
#define SHADOWMAP_CASCADES 4

    struct CsmPassData
    {
        Texture* shadowMap[SHADOWMAP_CASCADES];
        std::shared_ptr<Buffer> directLightBuffer;
    };

    struct CsmRC
    {
        glm::mat4 localModel{};
        glm::mat4 projView{};
        uint32_t cascadeIndex{};
    };

    struct DirectLightBuffer
    {
        glm::vec4 lightDirectionIntensity = glm::vec4(20.0f, 50, 20.0f, 2.0f);
        glm::mat4 viewProj[SHADOWMAP_CASCADES];
        float cascadeDistance[SHADOWMAP_CASCADES];
    };

    class CascadedShadowMaps : public RenderFeature
    {
      public:
        CascadedShadowMaps();
        ~CascadedShadowMaps() {};

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;

      private:
        std::vector<glm::vec3> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
        glm::mat4 GetCascadeMatrix(const glm::vec3& lightDir, const glm::mat4& cameraView, const glm::mat4& cascadeProj);

        DirectLightBuffer m_directLight;
        CsmRC m_rc{};
        std::shared_ptr<Buffer> m_directionalLightBuffer{};

        bool m_lightChanged = true;

        // Debug values for frustum drawing
        bool m_lockFrustum = false;
        std::vector<glm::vec3> m_maxExtents;
        std::vector<glm::vec3> m_minExtents;
        std::vector<glm::vec3> m_lightDirDebug;
        std::vector<glm::vec3> m_lightDirDebug2;
        std::vector<glm::mat4> m_lightView;
        std::vector<std::vector<glm::vec3>> m_frustumCorners;

        float m_zMult = 3.4;
    };
} // namespace Wild
