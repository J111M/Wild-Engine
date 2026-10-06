#include "Renderer/Resources/SceneData.hpp"

#include "Tools/BufferAllocator.hpp"

#include <cstring>

namespace Wild
{
    SceneCameraData SceneCameraData::FromCamera(const Camera& camera)
    {
        SceneCameraData data{};

        data.view = camera.GetView();
        data.projection = camera.GetProjection();
        data.viewProjection = data.projection * data.view;

        data.inverseView = glm::inverse(data.view);
        data.inverseProjection = glm::inverse(data.projection);
        data.inverseViewProjection = glm::inverse(data.viewProjection);

        data.position = glm::vec4(camera.GetPosition(), 1.0f);
        data.nearFarFovAspect = glm::vec4(camera.GetNearFar(), camera.GetFOV(), camera.GetAspect());

        data.frustum = camera.GetFrustum();

        return data;
    }

    SceneDataBuffer::SceneDataBuffer()
    {
        BufferDesc desc{};
        desc.size = SLOT_SIZE * SLOT_COUNT;
        desc.usage = BufferUsage::Constant;
        desc.access = MemoryAccess::CpuToGpu;
        desc.name = "Scene camera data";

        m_resource = engine.GetGfxContext()->GetBufferAllocator()->CreateBuffer(desc);
        m_resource->Handle()->SetName(L"Scene camera data");

        // Upload heap memory can stay mapped for the lifetime of the resource, the CPU never reads it back
        CD3DX12_RANGE readRange(0, 0);
        void* mapped = nullptr;
        ThrowIfFailed(m_resource->Handle()->Map(0, &readRange, &mapped), "Failed to map the scene camera data buffer");
        m_mappedData = static_cast<uint8_t*>(mapped);

        // Fill every slot with valid default data so a pass never reads uninitialized memory
        for (uint32_t slot = 0; slot < SLOT_COUNT; slot++)
        {
            std::memcpy(m_mappedData + slot * SLOT_SIZE, &m_data, sizeof(SceneCameraData));
        }
    }

    SceneDataBuffer::~SceneDataBuffer()
    {
        if (m_mappedData) m_resource->Handle()->Unmap(0, nullptr);
    }

    bool SceneDataBuffer::Update(const SceneCameraData& data, uint32_t frameIndex)
    {
        if (frameIndex >= BACK_BUFFER_COUNT)
        {
            WD_ERROR("Scene data frame index out of range: {}", frameIndex);
            return false;
        }

        m_activeSlot = frameIndex;
        m_data = data;

        std::memcpy(m_mappedData + m_activeSlot * SLOT_SIZE, &m_data, sizeof(SceneCameraData));
        return true;
    }

    D3D12_GPU_VIRTUAL_ADDRESS SceneDataBuffer::GetGpuAddress() const
    {
        return m_resource->GetGPUAddress() + m_activeSlot * SLOT_SIZE;
    }
} // namespace Wild
