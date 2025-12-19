#pragma once

#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/Resources/Texture.hpp"

#include "Renderer/RenderGraph/RenderGraph.hpp"

#include "Systems/GrassCompute.hpp"
#include "Systems/GrassManager.hpp"

#include "Core/Transform.hpp"

#include "Tools/Common3d12.hpp"
#include "Tools/States.hpp"

namespace Wild {
	struct RootConstant {
		glm::mat4 matrix{};
		uint32_t view{};
	};

	class Renderer;

	class RenderFeature : public NonCopyable
	{
	public:
		virtual void Add(Renderer& renderer, RenderGraph& rg) = 0;
		virtual ~RenderFeature() = default;
	};

	class Renderer
	{
	public:
		Renderer();
		~Renderer() {};
		
		void Update(CommandList& list);
		void Render(CommandList& list, float deltaTime);
	private:
		void CreateRootSignature();

		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		std::shared_ptr<Buffer> m_vertBuffer;

		RootConstant m_rc;

		PipelineStateSettings m_settings;

		//ComPtr<ID3D12RootSignature> m_rootSignature;

		//ComPtr<ID3D12PipelineState> m_pso;
		std::unique_ptr<GrassCompute> m_grassPreCompute;
		std::unique_ptr<GrassManager> m_grassManager;
		std::shared_ptr<Buffer> m_grassBuffer;

		std::shared_ptr<PipelineState> m_pipeline;

		std::unique_ptr<Texture> m_texture;
	};
}