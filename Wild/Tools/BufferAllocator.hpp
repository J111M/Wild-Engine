#pragma once

#include "Tools/D3D12Common.hpp"

#include "Renderer/Resources/BufferTypes.hpp"
#include "Renderer/Resources/D3D12Resource.hpp"

#include <D3D12MemAlloc.h>

#include <memory>
#include <string>

namespace Wild
{
    // A GPU resource together with the memory allocation that backs it.
    // The allocation must stay alive for as long as the resource is used.
    struct AllocatedResource
    {
        ComPtr<ID3D12Resource> resource;
        ComPtr<D3D12MA::Allocation> allocation;
    };

    // Owns access to the D3D12 Memory Allocator and creates buffer resources from it.
    // This centralises heap, resource flag and initial state selection so buffer
    // classes only have to describe intent through BufferUsage and MemoryAccess.
    class BufferAllocator : private NonCopyable
    {
      public:
        explicit BufferAllocator(ComPtr<D3D12MA::Allocator> allocator);

        // Creates a buffer from a high level description, deriving the heap type,
        // resource flags and initial state from the usage and access fields.
        std::unique_ptr<D3D12Resource> CreateBuffer(const BufferDesc& desc);

        // Creates a raw buffer on an explicit heap in an explicit state.
        // Used for transient upload and readback staging resources.
        AllocatedResource AllocateRaw(D3D12_HEAP_TYPE heapType, uint64_t sizeBytes, D3D12_RESOURCE_FLAGS flags,
                                      D3D12_RESOURCE_STATES initialState, const std::string& name);

      private:
        static D3D12_HEAP_TYPE HeapType(MemoryAccess access);
        static D3D12_RESOURCE_FLAGS ResourceFlags(BufferUsage usage);
        static D3D12_RESOURCE_STATES InitialState(BufferUsage usage, MemoryAccess access);

        ComPtr<D3D12MA::Allocator> m_allocator;
    };
} // namespace Wild
