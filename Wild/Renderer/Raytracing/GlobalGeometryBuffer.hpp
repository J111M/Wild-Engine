#pragma once

namespace Wild
{
    /// <summary>
    /// Geometry buffer based on vector class
    /// </summary>
    class GlobalGeometryBuffer
    {
      public:
        void Init(UINT64 initialCapacity, std::string debugName);

        UINT64 Reserve(UINT64 byteCount, UINT64 alignment = 4);

        ID3D12Resource* Handle() const { return m_resource.Get(); }
        D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const { return m_resource->GetGPUVirtualAddress(); }

        UINT64 UsedBytes() const { return m_head; }
        UINT64 CapacityBytes() const { return m_capacity; }

      private:
        void Grow(UINT64 newCapacity);
        void CopyBufferData(ID3D12Resource* src, ID3D12Resource* dst, UINT64 byteCount);

        ComPtr<ID3D12Resource> m_resource;
        UINT64 m_capacity = 0;
        UINT64 m_head = 0;
    };
} // namespace Wild
