#include "Renderer/Resources/Buffer.hpp"

#include "Renderer/Renderer.hpp"
#include "Renderer/Resources/Mesh.hpp"

namespace Wild
{
    GPUBuffer::GPUBuffer(const BufferDesc& desc)
    {
        m_desc = desc;

        // Constant buffers must be aligned to 256 bytes, keep the view in sync with the allocation
        if (HasFlag(m_desc.usage, BufferUsage::Constant)) m_desc.size = (m_desc.size + 255) & ~static_cast<uint64_t>(255);

        m_dataSize = static_cast<uint32_t>(m_desc.size);

        // Vertex and index buffers are created later once their data is known
        if (HasFlag(m_desc.usage, BufferUsage::Vertex) || HasFlag(m_desc.usage, BufferUsage::Index)) return;

        // Nothing to allocate yet if no size was supplied
        if (m_desc.size == 0) return;

        m_resource = engine.GetGfxContext()->GetBufferAllocator()->CreateBuffer(m_desc);

        if (!m_desc.name.empty()) m_resource->Handle()->SetName(StringToWString(m_desc.name).c_str());

        CreateViews();
    }

    GPUBuffer::~GPUBuffer() { Unmap(); }

    void GPUBuffer::CreateViews()
    {
        const uint32_t stride = m_desc.stride;
        const uint32_t numElements = stride ? static_cast<uint32_t>(m_desc.size / stride) : 1;

        if (HasFlag(m_desc.usage, BufferUsage::Constant))
        {
            m_cbView = std::make_shared<ConstantBufferView>(m_resource->Handle(), static_cast<uint32_t>(m_desc.size));
        }

        // A stride of zero means the buffer is used only through its GPU address (AC scratch and result buffers)
        if (HasFlag(m_desc.usage, BufferUsage::ShaderWrite) && stride > 0)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.Buffer.FirstElement = 0;
            uavDesc.Buffer.NumElements = numElements;
            uavDesc.Buffer.StructureByteStride = stride;

            m_uaView = std::make_shared<UnorderedAccessView>(m_resource->Handle(), uavDesc);
        }

        if (HasFlag(m_desc.usage, BufferUsage::ShaderRead) && stride > 0)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = numElements;
            srvDesc.Buffer.StructureByteStride = stride;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

            m_srView = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
        }
    }

    void GPUBuffer::CreateIndexBuffer(const std::vector<uint32_t>& indices)
    {
        auto gfxContext = engine.GetGfxContext();
        auto bufferAllocator = gfxContext->GetBufferAllocator();

        m_desc.stride = sizeof(uint32_t);
        m_desc.size = indices.size() * sizeof(uint32_t);
        m_dataSize = static_cast<uint32_t>(m_desc.size);

        m_resource = bufferAllocator->CreateBuffer(m_desc);

        // Upload staging resource, kept alive until the copy has finished at the end of this scope
        AllocatedResource upload = bufferAllocator->AllocateRaw(D3D12_HEAP_TYPE_UPLOAD,
                                                                m_desc.size,
                                                                D3D12_RESOURCE_FLAG_NONE,
                                                                D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                "Upload resource: " + m_desc.name);

        D3D12_SUBRESOURCE_DATA indexData = {};
        indexData.pData = indices.data();
        indexData.RowPitch = m_desc.size;
        indexData.SlicePitch = indexData.RowPitch;

        auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

        UpdateSubresources(list.GetList().Get(), m_resource->Handle().Get(), upload.resource.Get(), 0, 0, 1, &indexData);

        m_resource->Transition(list, D3D12_RESOURCE_STATE_INDEX_BUFFER);

        // Execute the command list
        list.Close();
        gfxContext->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
        gfxContext->GetCommandQueue(QueueType::Direct)->WaitForFence();

        m_ibView =
            std::make_shared<IndexBufferView>(m_resource->Handle(), static_cast<uint32_t>(m_desc.size), DXGI_FORMAT_R32_UINT);

        // Optional raw byte address SRV for bindless index access
        if (HasFlag(m_desc.usage, BufferUsage::ShaderRead))
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = 0;
            srvDesc.Buffer.NumElements = static_cast<UINT>(m_desc.size / 4); // R32 raw
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

            m_srView = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
        }
    }

    // Allocate data
    void GPUBuffer::Allocate(void* dataSrc, size_t size)
    {
        Map();
        WriteData(dataSrc, size);
        Unmap();
    }

    // Upload data straight to a GPU buffer
    void GPUBuffer::UploadToGPU(void* dataSrc, size_t size)
    {
        auto gfxContext = engine.GetGfxContext();
        auto bufferAllocator = gfxContext->GetBufferAllocator();

        size_t uploadSize = static_cast<size_t>(m_desc.size);

        // Upload staging buffer
        AllocatedResource upload = bufferAllocator->AllocateRaw(D3D12_HEAP_TYPE_UPLOAD,
                                                                uploadSize,
                                                                D3D12_RESOURCE_FLAG_NONE,
                                                                D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                "Upload staging: " + m_desc.name);

        // Copy data into the upload buffer
        void* mapped = nullptr;
        upload.resource->Map(0, nullptr, &mapped);
        memcpy(mapped, dataSrc, uploadSize);
        upload.resource->Unmap(0, nullptr);

        auto list = CommandList(D3D12_COMMAND_LIST_TYPE_DIRECT);

        auto oldState = m_resource->GetCurrentState();

        m_resource->Transition(list, D3D12_RESOURCE_STATE_COPY_DEST);

        list.GetList()->CopyBufferRegion(m_resource->Handle().Get(), 0, upload.resource.Get(), 0, uploadSize);

        m_resource->Transition(list, oldState);

        list.Close();
        gfxContext->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
        gfxContext->GetCommandQueue(QueueType::Direct)->WaitForFence();
    }

    void GPUBuffer::Transition(CommandList& list, D3D12_RESOURCE_STATES newState) { m_resource->Transition(list, newState); }

    void GPUBuffer::Map(CD3DX12_RANGE* readRange)
    {
        m_dataIsMapped = true;
        m_resource->Handle()->Map(0, readRange, &m_mappedData);
    }

    void GPUBuffer::Unmap()
    {
        if (m_dataIsMapped)
        {
            m_resource->Handle()->Unmap(0, nullptr);
            m_dataIsMapped = false;
            m_mappedData = nullptr;
        }
    }

    // Writes straight into the mapped upload memory, the buffer has to be mapped by the caller
    void GPUBuffer::WriteData(void* dataSrc, size_t size)
    {
        // TODO change to just use 1 standard instead of being able to overwrite it
        if (size > 0) m_desc.size = size;

        if (!m_dataIsMapped || !m_mappedData)
        {
            WD_ERROR("WriteData called on a buffer that is not mapped: {}", m_desc.name);
            return;
        }

        memcpy(m_mappedData, dataSrc, static_cast<size_t>(m_desc.size));
    }

    std::shared_ptr<VertexBufferView> GPUBuffer::GetVBView() const
    {
        if (m_vbView) { return m_vbView; }

        WD_WARN("Trying to access invalid vertex buffer view.");
        return nullptr;
    }

    std::shared_ptr<IndexBufferView> GPUBuffer::GetIBView() const
    {
        if (m_ibView) { return m_ibView; }

        WD_WARN("Trying to access invalid index buffer view.");
        return nullptr;
    }

    std::shared_ptr<ConstantBufferView> GPUBuffer::GetCBView() const
    {
        if (m_cbView) { return m_cbView; }

        WD_WARN("Trying to access invalid constant buffer view.");
        return nullptr;
    }

    std::shared_ptr<UnorderedAccessView> GPUBuffer::GetUAView() const
    {
        if (m_uaView) { return m_uaView; }

        WD_WARN("Trying to access invalid unordered access buffer view.");
        return nullptr;
    }

    std::shared_ptr<ShaderResourceView> GPUBuffer::GetSRView() const
    {
        if (m_srView) { return m_srView; }

        WD_WARN("Trying to access invalid shader resource view.");
        return nullptr;
    }
} // namespace Wild
