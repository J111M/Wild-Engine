#include "Tools/BufferAllocator.hpp"

namespace Wild
{
    BufferAllocator::BufferAllocator(ComPtr<D3D12MA::Allocator> allocator) : m_allocator(std::move(allocator)) {}

    D3D12_HEAP_TYPE BufferAllocator::HeapType(MemoryAccess access)
    {
        switch (access)
        {
        case MemoryAccess::CpuToGpu: return D3D12_HEAP_TYPE_UPLOAD;
        case MemoryAccess::GpuToCpu: return D3D12_HEAP_TYPE_READBACK;
        case MemoryAccess::GpuOnly:
        default: return D3D12_HEAP_TYPE_DEFAULT;
        }
    }

    D3D12_RESOURCE_FLAGS BufferAllocator::ResourceFlags(BufferUsage usage)
    {
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

        // Both unordered access buffers and acceleration structures need the UAV flag
        if (HasFlag(usage, BufferUsage::ShaderWrite) || HasFlag(usage, BufferUsage::AccelerationStruct))
        {
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        return flags;
    }

    D3D12_RESOURCE_STATES BufferAllocator::InitialState(BufferUsage usage, MemoryAccess access)
    {
        // Heap type dictates the required state for mappable buffers
        if (access == MemoryAccess::CpuToGpu) return D3D12_RESOURCE_STATE_GENERIC_READ;
        if (access == MemoryAccess::GpuToCpu) return D3D12_RESOURCE_STATE_COPY_DEST;

        // Default heap buffers, state derived from usage
        if (HasFlag(usage, BufferUsage::AccelerationStruct)) return D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

        // Vertex and index buffers are uploaded through a staging copy so they start as a copy destination
        if (HasFlag(usage, BufferUsage::Vertex) || HasFlag(usage, BufferUsage::Index)) return D3D12_RESOURCE_STATE_COPY_DEST;

        return D3D12_RESOURCE_STATE_COMMON;
    }

    AllocatedResource BufferAllocator::AllocateRaw(D3D12_HEAP_TYPE heapType, uint64_t sizeBytes, D3D12_RESOURCE_FLAGS flags,
                                                   D3D12_RESOURCE_STATES initialState, const std::string& name)
    {
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = heapType;

        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeBytes, flags);

        AllocatedResource result;

        ThrowIfFailed(m_allocator->CreateResource(&allocationDesc,
                                                  &resourceDesc,
                                                  initialState,
                                                  nullptr,
                                                  result.allocation.GetAddressOf(),
                                                  IID_PPV_ARGS(result.resource.GetAddressOf())),
                      "Failed to allocate buffer resource.");

        if (!name.empty()) result.resource->SetName(StringToWString(name).c_str());

        return result;
    }

    std::unique_ptr<D3D12Resource> BufferAllocator::CreateBuffer(const BufferDesc& desc)
    {
        uint64_t size = desc.size;

        // Constant buffers must be aligned to 256 bytes
        if (HasFlag(desc.usage, BufferUsage::Constant)) size = (size + 255) & ~static_cast<uint64_t>(255);

        const D3D12_HEAP_TYPE heapType = HeapType(desc.access);
        const D3D12_RESOURCE_FLAGS flags = ResourceFlags(desc.usage);
        const D3D12_RESOURCE_STATES state = InitialState(desc.usage, desc.access);

        AllocatedResource allocated = AllocateRaw(heapType, size, flags, state, desc.name);

        auto resource = std::make_unique<D3D12Resource>(state);
        resource->SetResource(allocated.resource);
        resource->SetAllocation(allocated.allocation);

        return resource;
    }
} // namespace Wild
