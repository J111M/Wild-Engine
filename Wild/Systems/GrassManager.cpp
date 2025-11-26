#include "Systems/GrassManager.hpp"

#include <vector>

namespace Wild {
	GrassManager::GrassManager()
	{
		auto device = engine.GetDevice();

		m_vertShader = std::make_shared<Shader>("Shaders/vertShader.hlsl");
		m_fragShader = std::make_shared<Shader>("Shaders/fragShader.hlsl");

		m_settings.ShaderState.VertexShader = m_vertShader;
		m_settings.ShaderState.FragShader = m_fragShader;
		m_settings.DepthStencilState.DepthEnable = true;

		// Setting up the input layout
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0));
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("COORDS", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3)));
		m_settings.ShaderState.InputLayout.emplace_back(InputElement("SWAY", DXGI_FORMAT_R32_FLOAT, sizeof(glm::vec3) + sizeof(float)));

		std::vector<Uniform> uniforms;

		Uniform uni{ 0, 0, RootParams::RootResourceType::Constants, sizeof(RootConstant) };

		uniforms.emplace_back(uni);

		m_pipeline = std::make_shared<PipelineState>(PipelineStateType::Graphics, m_settings, uniforms);

		std::vector<GrassVertex> grassBlade{};

		// Grass blade vertice data | position, 1D coordinates and sway
		grassBlade.push_back({ {-0.1,0.0,0.0}, 0.0, 0.0});
		grassBlade.push_back({ {0.1,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {-0.1,1.0,0.0}, 0.0, 0.0 });

		grassBlade.push_back({ {0.1,0.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.1,1.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {-0.1,1.0,0.0}, 0.0, 0.0 });

		grassBlade.push_back({ {-0.1,1.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.1,1.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {-0.1,1.5,0.0}, 0.0, 0.0 });

		grassBlade.push_back({ {0.1,1.0,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.1,1.5,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {-0.1,1.5,0.0}, 0.0, 0.0 });

		grassBlade.push_back({ {-0.1,1.5,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.1,1.5,0.0}, 0.0, 0.0 });
		grassBlade.push_back({ {0.0,2.3,0.0}, 0.0, 0.0 });

		BufferDesc desc{};
		m_grassBuffer = std::make_shared<Buffer>(desc);
		m_grassBuffer->CreateVertexBuffer<GrassVertex>(grassBlade);

	}
	GrassManager::~GrassManager()
	{
	}

	void GrassManager::Render(CommandList& list) {
		list.GetList()->IASetVertexBuffers(0, 1, &m_grassBuffer->GetVBView()->View());
		list.GetList()->DrawInstanced(15, 1, 0, 0);
	}
}