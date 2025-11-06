#pragma once

#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/ShaderPipeline.hpp"

#include "Tools/Common3d12.hpp"

namespace Wild {
	struct Vertex {
		glm::vec3 position{};
	};

	struct RootConstant {
		glm::mat4 matrix;
	};

	class Renderer
	{
	public:
		Renderer();
		~Renderer() {};
		
		void update();
		void render(CommandList& command_list);
	private:
		void CreateRootSignature();

		std::shared_ptr<Shader> m_vertShader;
		std::shared_ptr<Shader> m_fragShader;

		std::shared_ptr<Buffer> m_vertBuffer;

		RootConstant m_rc;

		ComPtr<ID3D12RootSignature> root_signature;

		ComPtr<ID3D12PipelineState> m_pso;
	};
}