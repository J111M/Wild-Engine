#pragma once

#include "Renderer/CommandList.hpp"
#include "Renderer/Resources/D3D12Resource.hpp"

#include "Tools/BufferAllocator.hpp"
#include "Tools/D3D12Common.hpp"
#include "Tools/D3D12Views.hpp"

#include "Renderer/Resources/BufferTypes.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Wild
{
    struct Vertex;

    class GPUBuffer
    {
      public:
        explicit GPUBuffer(const BufferDesc& desc);
        ~GPUBuffer();

        void CreateIndexBuffer(std::vector<uint32_t> indices);

        void Allocate(void* dataSrc, size_t size = 0);
        void UploadToGPU(void* dataSrc, size_t size = 0);

        void Map(CD3DX12_RANGE* readRange = nullptr);
        void Unmap();

        void WriteData(void* dataSrc, size_t size = 0);

        ID3D12Resource* GetBuffer() { return m_resource->Handle().Get(); }
        void Transition(CommandList& list, D3D12_RESOURCE_STATES newState);

        std::shared_ptr<VertexBufferView> GetVBView() const;
        std::shared_ptr<IndexBufferView> GetIBView() const;
        std::shared_ptr<ConstantBufferView> GetCBView() const;
        std::shared_ptr<UnorderedAccessView> GetUAView() const;
        std::shared_ptr<ShaderResourceView> GetSRView() const;

      private:
        BufferDesc m_desc;

        // Builds the CBV / SRV / UAV descriptor views that match the requested usage
        void CreateViews();

        std::unique_ptr<D3D12Resource> m_resource;

        bool m_dataIsMapped = false;

        // Vertex buffer view
        std::shared_ptr<VertexBufferView> m_vbView;
        std::shared_ptr<IndexBufferView> m_ibView;
        std::shared_ptr<ConstantBufferView> m_cbView;
        std::shared_ptr<UnorderedAccessView> m_uaView;

        // Srv for bindless raytracing heap
        std::shared_ptr<ShaderResourceView> m_srView;

        void* m_data = nullptr;

        uint32_t m_dataSize{};

      public:
        template <typename T> void CreateVertexBuffer(const std::vector<T>& vertices)
        {
            if (vertices.size() <= 0)
            {
                WD_ERROR("No buffer data supplied at resource creation!");
                return;
            }

            uint32_t stride = static_cast<uint32_t>(sizeof(T));

            if (stride == 0)
            {
                WD_ERROR("No valid stride supplied at resource creation for vertex buffer!");
                return;
            }

            m_desc.stride = stride;
            m_desc.size = vertices.size() * stride;
            m_dataSize = static_cast<uint32_t>(m_desc.size);

            auto gfxContext = engine.GetGfxContext();
            auto bufferAllocator = gfxContext->GetBufferAllocator();

            // Default heap resource that will hold the vertex data
            m_resource = bufferAllocator->CreateBuffer(m_desc);
            m_resource->Handle()->SetName(std::wstring(m_desc.name.begin(), m_desc.name.end()).c_str());

            // Upload staging resource, kept alive until the copy has finished at the end of this scope
            std::string uploadResourceName = "Upload resource: " + m_desc.name;
            AllocatedResource upload = bufferAllocator->AllocateRaw(D3D12_HEAP_TYPE_UPLOAD,
                                                                    m_desc.size,
                                                                    D3D12_RESOURCE_FLAG_NONE,
                                                                    D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                    uploadResourceName);

            WriteData((void*)vertices.data(), m_desc.size);

            D3D12_SUBRESOURCE_DATA data = {};
            data.pData = reinterpret_cast<BYTE*>(m_data);
            data.RowPitch = m_desc.size;
            data.SlicePitch = m_desc.size;

            auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

            UpdateSubresources(list.GetList().Get(), m_resource->Handle().Get(), upload.resource.Get(), 0, 0, 1, &data);

            auto state = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            if (HasFlag(m_desc.usage, BufferUsage::ShaderRead)) state |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            m_resource->Transition(list, state);

            // Execute the command list
            list.Close();
            gfxContext->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
            gfxContext->GetCommandQueue(QueueType::Direct)->WaitForFence();

            m_vbView =
                std::make_shared<VertexBufferView>(m_resource->Handle(), static_cast<uint32_t>(m_desc.size), stride);
            CreateViews();
        }
    };
} // namespace Wild
