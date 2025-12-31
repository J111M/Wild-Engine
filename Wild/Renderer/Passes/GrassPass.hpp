#pragma once

#include "Renderer/Renderer.hpp"
#include "Renderer/RenderGraph/RenderGraph.hpp"

#define MAXGRASSBLADES1 5000

namespace Wild {
	struct GrassCullData {
		std::shared_ptr<Buffer> CulledBuffer = nullptr;
	};

	struct RenderGrassData {
		Texture* FinalTexture;
		Texture* DepthTexture;
	};

	struct FrustumBuffer {
		glm::mat4 viewProj{};
		glm::vec4 frustumPlanes[6]{};
		glm::vec3 cameraPos{};
		float lod0{};
		float lod1{};
		float lod2{};
		float lodBlendRange{};
		float maxDistance{};
	};

	struct CulledInstance {
		uint32_t instanceIndex;
		float lodBlend;
	};

	struct GrassVertex {
		glm::vec3 Position{};
		float OneDCoordinates{};
		float Sway{};
	};

	struct SceneData {
		glm::mat4 ProjView{};
		glm::vec3 CameraPosition{};
	};

	struct GrassRC {
		glm::mat4 Matrix{};
		uint32_t bladeId{};
		float time;
		uint32_t foo2;
		uint32_t foo3;
	};

	struct ClearCounterData {
		float foo;
	};

	struct IndirectCommandsData {
		float foo;
	};

	class GrassPass : public RenderFeature
	{
	public:
		GrassPass(std::shared_ptr<Buffer> GrassBladeBuffer);
		~GrassPass() {};

		virtual void Add(Renderer& renderer, RenderGraph& rg) override;
		virtual void Update(const float dt) override;

	private:
		// Clear counter pass resets the instance count buffer to 0
		void AddClearCounterPass(Renderer& renderer, RenderGraph& rg);
		void AddGrassCulling(Renderer& renderer, RenderGraph& rg);
		void AddIndirectDrawCommandsPass(Renderer& renderer, RenderGraph& rg);
		void AddRenderGrass(Renderer& renderer, RenderGraph& rg);
		void UpdateFrustumData();

		void CreateGrassMeshes();

		// Store frustum data
		std::unique_ptr<Buffer> m_frustumBuffer;

		// Keeps track of all instances that need to be culled
		std::shared_ptr<Buffer> m_culledInstancesBuffer[BACK_BUFFER_COUNT];

		// Instance count buffer keeps track of the amount of instances that need to be drawn per LOD
		std::unique_ptr<Buffer> m_instanceCountBuffer[BACK_BUFFER_COUNT];

		// Draw command buffer stores the things that need to be drawn via execute indirect
		std::unique_ptr<Buffer> m_drawCommandsBuffer[BACK_BUFFER_COUNT];

		// Command signature for Execute indirect
		ComPtr<ID3D12CommandSignature> m_commandSignature;

		// Contains per blade grass data
		std::shared_ptr<Buffer> m_grassBladeInstanceBuffer;

		// Data for grass render pass
		GrassRC m_rc{};
		Entity m_chunkEntity;
		float m_accumulatedTime{};
		std::shared_ptr<Buffer> m_sceneData[BACK_BUFFER_COUNT];

		// Contains all LOD's inside the same buffer
		std::unique_ptr<Buffer> m_grassVertices;
		std::unique_ptr<Buffer> m_grassIndices;

		// 3 grass lod's total
		uint32_t m_lodAmount = 3;
	};
}