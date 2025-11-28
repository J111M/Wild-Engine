#pragma once

#include "Renderer/Resources/Buffer.hpp"
#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/CommandList.hpp"

#include <memory>

namespace Wild {
	class RenderPass
	{
	public:
		RenderPass() {};
		~RenderPass() {};

		void Update() {};
		void Render(CommandList& list) {};

	protected:
		std::shared_ptr<PipelineState> m_pipeline;
		PipelineStateSettings m_settings{};
	};
}