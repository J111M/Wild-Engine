#pragma once

#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/Resources/Texture.hpp"

#include "Renderer/RenderGraph/RenderGraph.hpp"

#include "Systems/GrassCompute.hpp"

#include "Core/Transform.hpp"

#include "Tools/Common3d12.hpp"
#include "Tools/States.hpp"

namespace Wild {
	struct RootConstant {
		glm::mat4 matrix{};
		glm::mat4 invMatrix{};
		uint32_t albedoView{};
		uint32_t normalView{};
		uint32_t roughnessMetallicView{};
		uint32_t emissiveView{};
	};

	class Renderer;

	class RenderFeature : public NonCopyable
	{
	public:
		virtual void Add(Renderer& renderer, RenderGraph& rg) = 0;
		virtual void Update(float dt) = 0;
		virtual ~RenderFeature() = default;
	};

	class Renderer
	{
	public:
		Renderer();
		~Renderer();
		
		void Update(const float dt);
		void Render(CommandList& list, float deltaTime);

		template <typename T>
		T* GetRenderFeature();

		bool HasPipelineInCache(const std::string& key);
		std::shared_ptr<PipelineState> GetOrCreatePipeline(const std::string& key, PipelineStateType Type, const PipelineStateSettings& settings, const std::vector<Uniform>& uniforms = {});
		std::shared_ptr<PipelineState> GetPipeline(const std::string& key);

		Texture* irradianceMap{};
		Texture* specularMap{};
		Texture* brdfLut{};
		Texture* compositeTexture = nullptr;
	private:
		
		std::vector<std::unique_ptr<RenderFeature>> m_renderFeatures;
		std::shared_ptr<TransientResourceCache> m_resourceCache;

		std::unique_ptr<GrassCompute> m_grassPreCompute;

		std::unordered_map<std::string, std::shared_ptr<PipelineState>> m_pipelineCache;

	};

	template<typename T>
	inline T* Renderer::GetRenderFeature()
	{
		for (auto& feature : m_renderFeatures)
		{
			T* casted = dynamic_cast<T*>(feature.get());
			if (casted)
			{
				return casted;
			}
		}

		// Return nullptr if feature is not found
		return nullptr;
	}
}