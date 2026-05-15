#include "Renderer/Raytracing/GlobalGeometryBuffer.hpp"

namespace Wild
{
    void GlobalGeometryBuffer::Init(UINT64 initialCapacity, std::string debugName)
    {
        Grow(initialCapacity);
        m_resource->SetName(StringToWString(debugName).c_str());
    }

    UINT64 GlobalGeometryBuffer::Reserve(UINT64 byteCount, UINT64 alignment)
    {
        UINT64 aligned = (m_head + alignment - 1) & ~(alignment - 1);
        if (aligned + byteCount > m_capacity)
        {
            Grow(std::max<UINT64>((aligned + byteCount) * 2, m_capacity * 2));
            aligned = (m_head + alignment - 1) & ~(alignment - 1);
        }
        UINT64 offset = aligned;
        m_head = aligned + byteCount;
        return offset;
    }

    void GlobalGeometryBuffer::Grow(UINT64 newCapacity)
    {
        // Keep the old buffer alive
        ComPtr<ID3D12Resource> oldResource = m_resource;

        CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(newCapacity);

        engine.GetGfxContext()->GetDevice()->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource));

        if (oldResource && m_head > 0) { CopyBufferData(oldResource.Get(), m_resource.Get(), m_head); }

        m_capacity = newCapacity;
    }

    void GlobalGeometryBuffer::CopyBufferData(ID3D12Resource* src, ID3D12Resource* dst, UINT64 byteCount)
    {
        auto gfxContext = engine.GetGfxContext();
        auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

        CD3DX12_RESOURCE_BARRIER preBarriers[] = {
            CD3DX12_RESOURCE_BARRIER::Transition(
                src, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE),
            CD3DX12_RESOURCE_BARRIER::Transition(dst, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
        };

        list.GetList()->ResourceBarrier(_countof(preBarriers), preBarriers);

        list.GetList()->CopyBufferRegion(dst, 0, src, 0, byteCount);

        CD3DX12_RESOURCE_BARRIER postBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
            dst, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        list.GetList()->ResourceBarrier(1, &postBarrier);

        list.Close();

        auto queue = gfxContext->GetCommandQueue(QueueType::Direct);
        queue->ExecuteList(list);
        queue->WaitForFence();
    }
} // namespace Wild
