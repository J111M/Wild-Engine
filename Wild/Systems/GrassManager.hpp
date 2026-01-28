#pragma once

#include "Renderer/RenderGraph/RenderGraph.hpp"
#include "Renderer/Renderer.hpp"

#include "Core/ECS.hpp"

#include <memory>

namespace Wild
{
    /*struct GrassVertex {
        glm::vec3 Position{};
        float OneDCoordinates{};
        float Sway{};
    };

    struct SceneData {
        glm::mat4 ProjView{};
        glm::vec3 CameraPosition{};
    };

    struct GrassRC {
        glm::mat4 matrix{};
        uint32_t bladeId{};
        float time;
        uint32_t foo2;
        uint32_t foo3;
    };

    struct GrassPassData
    {
        Texture* FinalTexture;
        Texture* DepthTexture;
    };

    class GrassManager : public RenderFeature
    {
    public:
        GrassManager(std::shared_ptr<Buffer> GrassData);
        ~GrassManager();

        virtual void Add(Renderer& renderer, RenderGraph& rg) override;
        virtual void Update(const float dt) override;
    private:

        std::shared_ptr<Buffer> m_grassBuffer;
        std::shared_ptr<Buffer> m_sceneData[BACK_BUFFER_COUNT];

        std::shared_ptr<Buffer> m_grassDataBuffer;

        float m_accumulatedTime{};

        GrassRC m_rc{};
        Entity m_chunkEntity;
    };*/

}
