#pragma once

#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/ShaderPipeline.hpp"

#include "Core/Transform.hpp"

#include "Tools/Common3d12.hpp"

namespace Wild {
	struct RootConstant {
		glm::mat4 matrix{};
	};

	class Renderer
	{
	public:
		Renderer();
		~Renderer() {};
		
		void Update() {};
		void Render(CommandList& commandList);
	private:
		void CreateRootSignature();

		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		std::shared_ptr<Buffer> m_vertBuffer;

		RootConstant m_rc;

		PipelineStateSettings m_settings;

		ComPtr<ID3D12RootSignature> m_rootSignature;

		ComPtr<ID3D12PipelineState> m_pso;
	};
}