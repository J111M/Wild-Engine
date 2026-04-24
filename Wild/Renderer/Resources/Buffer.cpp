#include "Renderer/Resources/Buffer.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Mesh.hpp"

namespace Wild
{
    Buffer::Buffer(BufferDesc desc, BufferType type)
    {
        desc.type = type;
        m_desc = desc;

        m_dataSize = m_desc.bufferSize;

        switch (type)
        {
        case BufferType::constant:
            CreateConstantBuffer();
            break;
        case BufferType::uav:
            CreateUAVBuffer();
            break;
        case BufferType::shaderBindingTable:
            CreateSBTBuffer();
            break;
        default:
            break;
        }

        m_desc.bufferSize = m_dataSize;
    }

    Buffer::~Buffer() { Unmap(); }

    void Buffer::CreateConstantBuffer()
    {
        auto gfxContext = engine.GetGfxContext();

        if (m_desc.bufferSize <= 0)
        {
            WD_ERROR("Constant buffer invalid data size.");
            return;
        }

        m_desc.bufferSize = (m_desc.bufferSize + 255) & ~255;

        m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_GENERIC_READ);

        ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                                       D3D12_HEAP_FLAG_NONE,
                                                                       &CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize),
                                                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                       nullptr,
                                                                       IID_PPV_ARGS(&m_resource->Handle())),
                      "Failed to create constant buffer.");

        m_cbView = std::make_shared<ConstantBufferView>(m_resource->Handle(), m_desc.bufferSize);
    }

    void Buffer::CreateUAVBuffer()
    {
        auto gfxContext = engine.GetGfxContext();

        if (m_desc.bufferSize <= 0 || m_desc.numOfElements <= 0)
        {
            WD_ERROR("UAV buffer invalid data size or elements not specified.");
            return;
        }

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = m_desc.bufferSize * m_desc.numOfElements;
        desc.Height = m_desc.height;
        desc.DepthOrArraySize = m_desc.depth;
        desc.MipLevels = m_desc.mipLevels;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        m_resource = std::make_unique<Resource>(m_desc.state);

        gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &desc,
                                                         m_desc.state,
                                                         nullptr,
                                                         IID_PPV_ARGS(&m_resource->Handle()));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = m_desc.numOfElements;
        uavDesc.Buffer.StructureByteStride = m_desc.bufferSize;

        m_uaView = std::make_shared<UnorderedAccessView>(m_resource->Handle(), uavDesc);
        m_resource->Handle()->SetName(StringToWString(m_desc.name).c_str());
    }

    void Buffer::CreateSBTBuffer()
    {
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize);

        auto device7 = engine.GetGfxContext()->GetDevice7();

        m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_COMMON);

        if (device7)
        {
            device7->CreateCommittedResource(&heapProps,
                                             D3D12_HEAP_FLAG_NONE,
                                             &desc,
                                             D3D12_RESOURCE_STATE_GENERIC_READ,
                                             nullptr,
                                             IID_PPV_ARGS(&m_resource->Handle()));
        }
    }

    void Buffer::CreateIndexBuffer(std::vector<uint32_t> indices)
    {
        auto gfxContext = engine.GetGfxContext();

        m_desc.bufferSize = indices.size() * sizeof(uint32_t);

        WriteData((void*)indices.data(), m_desc.bufferSize);

        m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_COPY_DEST);

        gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize),
                                                         D3D12_RESOURCE_STATE_COMMON,
                                                         nullptr,
                                                         IID_PPV_ARGS(&m_resource->Handle()));

        // Upload buffer
        ComPtr<ID3D12Resource> uploadHeap;
        gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &CD3DX12_RESOURCE_DESC::Buffer(m_desc.bufferSize),
                                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                                         nullptr,
                                                         IID_PPV_ARGS(&uploadHeap));

        D3D12_SUBRESOURCE_DATA indexData = {};
        indexData.pData = m_data;
        indexData.RowPitch = m_desc.bufferSize;
        indexData.SlicePitch = indexData.RowPitch;

        auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

        UpdateSubresources(list.GetList().Get(), m_resource->Handle().Get(), uploadHeap.Get(), 0, 0, 1, &indexData);

        m_resource->Transition(list, D3D12_RESOURCE_STATE_INDEX_BUFFER);

        // Execute the command list
        list.Close();
        gfxContext->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
        gfxContext->GetCommandQueue(QueueType::Direct)->WaitForFence();

        m_ibView = std::make_shared<IndexBufferView>(m_resource->Handle(), m_desc.bufferSize, DXGI_FORMAT_R32_UINT);
    }

    // Allocate data
    void Buffer::Allocate(void* dataSrc, size_t size)
    {
        Map();
        WriteData(dataSrc, size);
        Unmap();
    }

    // Upload data straight to a GPU buffer
    void Buffer::UploadToGPU(void* dataSrc, size_t size)
    {
        // Upload staging buffer
        ComPtr<ID3D12Resource> uploadBuffer;

        size_t uploadSize = m_desc.bufferSize * m_desc.numOfElements;

        auto gfxContext = engine.GetGfxContext();

        gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                         D3D12_HEAP_FLAG_NONE,
                                                         &CD3DX12_RESOURCE_DESC::Buffer(uploadSize),
                                                         D3D12_RESOURCE_STATE_GENERIC_READ,
                                                         nullptr,
                                                         IID_PPV_ARGS(&uploadBuffer));

        // Copy data into the upload buffer
        void* mapped = nullptr;
        uploadBuffer->Map(0, nullptr, &mapped);
        memcpy(mapped, dataSrc, uploadSize);
        uploadBuffer->Unmap(0, nullptr);

        auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

        m_resource->Transition(list, D3D12_RESOURCE_STATE_COPY_DEST);

        list.GetList()->CopyBufferRegion(m_resource->Handle().Get(), 0, uploadBuffer.Get(), 0, uploadSize);

        m_resource->Transition(list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        list.Close();
        gfxContext->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
        gfxContext->GetCommandQueue(QueueType::Direct)->WaitForFence();
    }

    void Buffer::Transition(CommandList& list, D3D12_RESOURCE_STATES newState) { m_resource->Transition(list, newState); }

    void Buffer::Map(CD3DX12_RANGE* readRange)
    {
        m_dataIsMapped = true;
        m_resource->Handle()->Map(0, readRange, &m_data);
    }

    void Buffer::Unmap()
    {
        if (m_dataIsMapped)
        {
            m_resource->Handle()->Unmap(0, nullptr);
            m_dataIsMapped = false;
        }
    }

    void Buffer::WriteData(void* dataSrc, size_t size)
    {
        // TODO change to just use 1 standard instead of being able to overwrite it
        if (size > 0) m_desc.bufferSize = size;

        // Make sure the pointed has allocated data
        if (!m_data) m_data = malloc(m_desc.bufferSize);

        if (m_data) { memcpy(m_data, dataSrc, m_desc.bufferSize); }
    }

    std::shared_ptr<VertexBufferView> Buffer::GetVBView() const
    {
        if (m_vbView) { return m_vbView; }

        WD_WARN("Trying to access invalid vertex buffer view.");
        return nullptr;
    }

    std::shared_ptr<IndexBufferView> Buffer::GetIBView() const
    {
        if (m_ibView) { return m_ibView; }

        WD_WARN("Trying to access invalid index buffer view.");
        return nullptr;
    }

    std::shared_ptr<ConstantBufferView> Buffer::GetCBView() const
    {
        if (m_cbView) { return m_cbView; }

        WD_WARN("Trying to access invalid constant buffer view.");
        return nullptr;
    }

    std::shared_ptr<UnorderedAccessView> Buffer::GetUAView() const
    {
        if (m_uaView) { return m_uaView; }

        WD_WARN("Trying to access invalid unordered access buffer view.");
        return nullptr;
    }
} // namespace Wild
