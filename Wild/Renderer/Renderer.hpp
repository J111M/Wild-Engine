#pragma once

#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/Resources/SceneData.hpp"
#include "Renderer/Resources/Texture.hpp"
#include "Renderer/ShaderPipeline.hpp"

#include "Renderer/RenderGraph/RenderGraph.hpp"

#include "Core/Camera.hpp"
#include "Core/Transform.hpp"

#include "Systems/SystemManager.hpp"

#include "Tools/D3D12Common.hpp"
#include "Tools/States.hpp"

namespace Wild
{
    class Renderer;

    class RenderFeature : public NonCopyable
    {
      public:
        virtual void Add(Renderer& renderer, RenderGraph& rg) = 0;
        virtual void Update(float dt) = 0;
        virtual ~RenderFeature() = default;

        // Camera data of the current frame
        const SceneCameraData& GetSceneData() const;
        D3D12_GPU_VIRTUAL_ADDRESS GetSceneDataAddress() const;
    };

    class Renderer
    {
      public:
        Renderer();
        ~Renderer();

        void Update(const float dt);
        void Render(CommandList& list, float deltaTime);

        template <typename T> T* GetRenderFeature();

        SystemManager& GetSystems() { return m_systemManager; }

        bool HasPipelineInCache(const std::string& key);
        std::shared_ptr<PipelineState> GetOrCreatePipeline(const std::string& key, PipelineStateType Type,
                                                           PipelineStateSettings& settings,
                                                           const std::vector<Uniform>& uniforms = {});
        std::shared_ptr<PipelineState> GetPipeline(const std::string& key);

        void FlushResources();

        void AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color = {1, 1, 1});
        void AddAABB(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color = {1, 1, 1});

        void CacheIBLTextures();

        // Camera data of the current frame, written at the start of Render
        const SceneCameraData& GetSceneData() const { return m_sceneData->GetData(); }
        D3D12_GPU_VIRTUAL_ADDRESS GetSceneDataAddress() const { return m_sceneData->GetGpuAddress(); }

        // IBL textures
        Texture* irradianceMap{};
        Texture* specularMap{};
        Texture* brdfLut{};

        Texture* environmentMap{};

        Texture* compositeTexture = nullptr;

        // Output is overwritten if set regardless of pass order
        Texture* compositeOverride = nullptr;

      private:
        std::unique_ptr<SceneDataBuffer> m_sceneData;

        SystemManager m_systemManager;

        std::vector<std::unique_ptr<RenderFeature>> m_renderFeatures;
        std::shared_ptr<TransientResourceCache> m_resourceCache;

        std::unordered_map<std::string, std::shared_ptr<PipelineState>> m_pipelineCache;

        bool m_texturesCached = false;
    };

    template <typename T> inline T* Renderer::GetRenderFeature()
    {
        for (auto& feature : m_renderFeatures)
        {
            T* casted = dynamic_cast<T*>(feature.get());
            if (casted) { return casted; }
        }

        // Return nullptr if feature is not found
        return nullptr;
    }
} // namespace Wild
