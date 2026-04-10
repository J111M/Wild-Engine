#pragma once

#include "Renderer/CommandList.hpp"
#include "Renderer/Resources/Resource3d12.hpp"

#include "Tools/Common3d12.hpp"
#include "Tools/View3d12.hpp"

#include <string>
#include <vector>

namespace Wild
{
    struct Vertex;

    enum BufferFlag : uint32_t
    {
        none = 0,
        shaderResource = 1 << 1,
        readWrite = 1 << 2,
        cpuResource = 1 << 3,
    };

    enum BufferType : uint32_t
    {
        default = 0,
        constant = 1 << 1,
        uav = 1 << 2,
        shaderBindingTable = 1 << 3,
    };

    struct BufferDesc
    {
        BufferFlag flag = BufferFlag::shaderResource;
        BufferType type = BufferType::default;

        uint32_t bufferSize{};
        uint32_t numOfElements{};

        UINT64 width{1};
        UINT height{1};
        UINT16 depth{1};
        UINT16 mipLevels{1};
        DXGI_FORMAT format;
        // D3D12_RESOURCE_FLAGS flags;

        std::string name = "default";
    };

    // TODO rework buffer class so that the resource creation is done in the initializer
    class Buffer
    {
      public:
        Buffer(BufferDesc desc, BufferType type = BufferType::default);
        ~Buffer();

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

      private:
        BufferDesc m_desc;

        void CreateConstantBuffer();
        void CreateUAVBuffer();
        void CreateSBTBuffer();

        std::unique_ptr<Resource> m_resource;

        bool m_dataIsMapped = false;

        // Vertex buffer view
        std::shared_ptr<VertexBufferView> m_vbView;
        std::shared_ptr<IndexBufferView> m_ibView;
        std::shared_ptr<ConstantBufferView> m_cbView;
        std::shared_ptr<UnorderedAccessView> m_uaView;

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
            m_desc.bufferSize = vertices.size() * stride;

            if (stride == 0)
            {
                WD_ERROR("No valid stride supplied at resource creation for vertex buffer!");
                return;
            }

            auto gfxContext = engine.GetGfxContext();

            m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_COPY_DEST);

            gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                             D3D12_HEAP_FLAG_NONE,
                                                             &CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize),
                                                             D3D12_RESOURCE_STATE_COMMON,
                                                             nullptr,
                                                             IID_PPV_ARGS(&m_resource->Handle()));

            m_resource->Handle()->SetName(std::wstring(m_desc.name.begin(), m_desc.name.end()).c_str());

            // Upload heap for GPU resources
            ComPtr<ID3D12Resource> uploadHeap;

            gfxContext->GetDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),  // upload heap
                D3D12_HEAP_FLAG_NONE,                              // no flags
                &CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize), // resource description for a buffer
                D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
                nullptr,
                IID_PPV_ARGS(&uploadHeap));

            std::string uploadResourceName = "Upload resource: " + m_desc.name;

            uploadHeap->SetName(std::wstring(uploadResourceName.begin(), uploadResourceName.end()).c_str());

            WriteData((void*)vertices.data(), m_desc.bufferSize);

            // Buffer type to upload heap
            D3D12_SUBRESOURCE_DATA data = {};
            data.pData = reinterpret_cast<BYTE*>(m_data);
            data.RowPitch = m_desc.bufferSize;
            data.SlicePitch = m_desc.bufferSize;

            auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

            UpdateSubresources(list.GetList().Get(), m_resource->Handle().Get(), uploadHeap.Get(), 0, 0, 1, &data);

            m_resource->Transition(list, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

            // Execute the command list
            list.Close();
            gfxContext->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
            gfxContext->GetCommandQueue(QueueType::Direct)->WaitForFence();

            m_vbView = std::make_shared<VertexBufferView>(m_resource->Handle(), m_desc.bufferSize, stride);
        }
    };
} // namespace Wild
