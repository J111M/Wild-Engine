#include "Systems/GrassManager.hpp"
#include "Renderer/Passes/DeferredPass.hpp"

#include <vector>

namespace Wild
{
    // GrassManager::GrassManager(std::shared_ptr<Buffer> GrassData)
    //{
    //	m_grassDataBuffer = GrassData;
    //	std::vector<GrassVertex> grassBlade{};

    //	// Grass blade vertice data | position, 1D coordinates and sway
    //	grassBlade.push_back({ {-0.06,0.0,0.0}, 0.0, 0.0 });
    //	grassBlade.push_back({ {0.06,0.0,0.0}, 0.0, 0.0 });
    //	grassBlade.push_back({ {-0.06,0.3,0.0}, 0.0, 0.4 });

    //	grassBlade.push_back({ {0.06,0.0,0.0}, 0.0, 0.0 });
    //	grassBlade.push_back({ {0.06,0.3,0.0}, 0.0, 0.4 });
    //	grassBlade.push_back({ {-0.06,0.3,0.0}, 0.0, 0.4 });

    //	grassBlade.push_back({ {-0.06,0.3,0.0}, 0.0, 0.4 });
    //	grassBlade.push_back({ {0.06,0.3,0.0}, 0.0, 0.4 });
    //	grassBlade.push_back({ {-0.06,0.5,0.0}, 0.0, 0.7 });

    //	grassBlade.push_back({ {0.06,0.3,0.0}, 0.0, 0.4 });
    //	grassBlade.push_back({ {0.06,0.5,0.0}, 0.0, 0.7 });
    //	grassBlade.push_back({ {-0.06,0.5,0.0}, 0.0, 0.7 });

    //	grassBlade.push_back({ {-0.06,0.5,0.0}, 0.0, 0.7 });
    //	grassBlade.push_back({ {0.06,0.5,0.0}, 0.0, 0.7 });
    //	grassBlade.push_back({ {0.0,0.7,0.0}, 0.0, 1.0 });

    //	{
    //		BufferDesc desc{};
    //		m_grassBuffer = std::make_shared<Buffer>(desc);
    //		m_grassBuffer->CreateVertexBuffer<GrassVertex>(grassBlade);
    //	}

    //	{
    //		BufferDesc desc{};
    //		desc.bufferSize = sizeof(SceneData);

    //		for (int i = 0; i < BACK_BUFFER_COUNT; i++)
    //		{
    //			m_sceneData[i] = std::make_shared<Buffer>(desc);
    //			m_sceneData[i]->CreateConstantBuffer();

    //			// Keep buffer data mapped for cpu write access
    //			CD3DX12_RANGE readRange(0, 0);
    //			m_sceneData[i]->Map(&readRange);
    //		}
    //	}

    //	m_chunkEntity = engine.GetECS()->CreateEntity();
    //	engine.GetECS()->AddComponent<Transform>(m_chunkEntity, glm::vec3(2, 0, -5), m_chunkEntity);
    //}

    // GrassManager::~GrassManager()
    //{

    //}

    // void GrassManager::Update(const float dt)
    //{
    //	auto& device = engine.GetGfxContext();
    //	auto& ecs = engine.GetECS();

    //	SceneData SceneCbv{};
    //	auto& cameras = ecs->View<Camera>();

    //	// TODO change to support multiple cameras I only have 1 for now
    //	for (auto& cameraEntity : cameras) {
    //		if (ecs->HasComponent<Camera>(cameraEntity)) {
    //			auto& cam = ecs->GetComponent<Camera>(cameraEntity);

    //			SceneCbv.ProjView = cam.GetProjection() * cam.GetView();
    //			SceneCbv.CameraPosition = cam.GetPosition();
    //		}
    //	}

    //	m_sceneData[device->GetBackBufferIndex()]->WriteData(&SceneCbv);

    //	m_accumulatedTime += dt * 1;
    //	m_rc.time = m_accumulatedTime;
    //}

    // void GrassManager::Add(Renderer& renderer, RenderGraph& rg)
    //{
    //	auto* grassPassData = rg.AllocatePassData<GrassPassData>();
    //	auto* deferredData = rg.GetPassData<GrassPassData, DeferredPassData>();

    //	grassPassData->FinalTexture = deferredData->FinalTexture;
    //	grassPassData->DepthTexture = deferredData->DepthTexture;

    //	rg.AddPass<GrassPassData>(
    //		"Grass Pass",
    //		PassType::Graphics,
    //		[&renderer, this](const GrassPassData& passData, CommandList& list)
    //		{
    //
    //		});
    //}

} // namespace Wild
