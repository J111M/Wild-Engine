#include "Renderer/Resources/Texture.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace Wild
{
    Texture::Texture(const std::string& filePath, TextureType type, uint32_t mips, DXGI_FORMAT format)
    {
        m_desc.name = filePath;
        m_desc.type = type;
        m_desc.format = format;

        switch (type)
        {
        case Wild::TextureType::TEXTURE_2D:
            CreateTexture(filePath);
            break;
        case Wild::TextureType::TEXTURE_1D:
            WD_WARN("Not supported yet.");
            // CreateTexture(filePath);
            break;
        case Wild::TextureType::CUBEMAP:
            CreateCubeMapTexture(filePath);
            break;
        case Wild::TextureType::SKYBOX:
            CreateSkyboxTexture(filePath);
            break;
        default:
            break;
        }

        m_resource->Handle()->SetName(StringToWString(m_desc.name).c_str());
    }

    Texture::Texture(const TextureDesc& desc)
    {
        m_desc = desc;

        auto gfxContext = engine.GetGfxContext();

        if (m_desc.flag & TextureDesc::renderTarget)
        {
            D3D12_RESOURCE_DESC textureDesc = {};

            switch (m_desc.type)
            {
            case Wild::TextureType::TEXTURE_3D:
                textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                break;
            case Wild::TextureType::TEXTURE_2D:
                textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                break;
            case Wild::TextureType::TEXTURE_1D:
                textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
                break;
            default:
                textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                break;
            }

            textureDesc.Width = m_desc.width;
            textureDesc.Height = m_desc.height;
            textureDesc.DepthOrArraySize = m_desc.depthOrArray;
            textureDesc.MipLevels = m_desc.mips;
            textureDesc.Format = m_desc.format;
            textureDesc.SampleDesc.Count = 1;
            textureDesc.SampleDesc.Quality = 0;
            textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            if (m_desc.flag & TextureDesc::readWrite) { textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = m_desc.format;
            clearValue.Color[0] = 0.0f;
            clearValue.Color[1] = 0.0f;
            clearValue.Color[2] = 0.0f;
            clearValue.Color[3] = 1.0f;

            // Create resource with initial state
            m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_RENDER_TARGET);

            ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                                           D3D12_HEAP_FLAG_NONE,
                                                                           &textureDesc,
                                                                           D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                           &clearValue,
                                                                           IID_PPV_ARGS(&m_resource->Handle())),
                          "Failed to create RTV");

            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = m_desc.format;

            switch (m_desc.type)
            {
            case Wild::TextureType::TEXTURE_3D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
                rtvDesc.Texture3D.MipSlice = 0;
                rtvDesc.Texture3D.FirstWSlice = 0;
                rtvDesc.Texture3D.WSize = m_desc.depthOrArray;
                break;
            case Wild::TextureType::TEXTURE_2D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                break;
            case Wild::TextureType::TEXTURE_1D:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                break;
            default:
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                break;
            }

            if (m_desc.depthOrArray > 1)
            {
                for (uint32_t face = 0; face < m_desc.depthOrArray; face++)
                {
                    rtvDesc.Texture2DArray.MipSlice = 0;
                    rtvDesc.Texture2DArray.FirstArraySlice = face;
                    rtvDesc.Texture2DArray.ArraySize = 1;

                    switch (m_desc.type)
                    {
                    case Wild::TextureType::TEXTURE_3D:
                        WD_WARN("Invalid texture array for 3D textures.");
                        break;
                    case Wild::TextureType::TEXTURE_2D:
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        break;
                    case Wild::TextureType::TEXTURE_1D:
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                        break;
                    default:
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        break;
                    }

                    m_rtvArray.emplace_back(std::make_shared<RenderTargetView>(m_resource->Handle(), rtvDesc));
                }
                m_rtvArrayAvailiable = true;
            }
            else
            {
                m_rtv = std::make_shared<RenderTargetView>(m_resource->Handle(), rtvDesc);
            }
        }

        if (desc.flag & TextureDesc::depthStencil)
        {
            D3D12_RESOURCE_DESC depthDesc = {};
            depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            depthDesc.Alignment = 0;
            depthDesc.Width = m_desc.width;
            depthDesc.Height = m_desc.height;
            depthDesc.DepthOrArraySize = 1;
            depthDesc.MipLevels = 1;
            depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            depthDesc.SampleDesc.Count = 1;
            depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            depthDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = DXGI_FORMAT_D32_FLOAT;
            clearValue.DepthStencil = {1.0f, 0};

            // Create resource with initial state
            m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_DEPTH_WRITE);

            gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                             D3D12_HEAP_FLAG_NONE,
                                                             &depthDesc,
                                                             D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                                             &clearValue,
                                                             IID_PPV_ARGS(&m_resource->Handle()));

            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

            m_dsv = std::make_shared<DepthStencilView>(m_resource->Handle(), dsvDesc);

            m_desc.format = DXGI_FORMAT_R32_FLOAT;
        }

        if (m_desc.flag & TextureDesc::ViewFlag::readWrite)
        {
            D3D12_RESOURCE_DESC readWriteDesc = {};

            switch (m_desc.type)
            {
            case Wild::TextureType::TEXTURE_3D:
                readWriteDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
                break;
            case Wild::TextureType::TEXTURE_2D:
                readWriteDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                break;
            case Wild::TextureType::TEXTURE_1D:
                readWriteDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
                break;
            default:
                readWriteDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                break;
            }

            readWriteDesc.Width = m_desc.width;
            readWriteDesc.Height = m_desc.height;
            readWriteDesc.DepthOrArraySize = m_desc.depthOrArray;
            readWriteDesc.MipLevels = m_desc.mips;
            readWriteDesc.Format = m_desc.format;
            readWriteDesc.SampleDesc.Count = 1;
            readWriteDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            readWriteDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            if (!m_resource)
            {
                // Create resource with initial state
                m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                                 D3D12_HEAP_FLAG_NONE,
                                                                 &readWriteDesc,
                                                                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                                 nullptr,
                                                                 IID_PPV_ARGS(&m_resource->Handle()));
            }

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = m_desc.format;

            switch (m_desc.type)
            {
            case Wild::TextureType::TEXTURE_3D:
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
                uavDesc.Texture3D.MipSlice = 0;
                uavDesc.Texture3D.FirstWSlice = 0;
                uavDesc.Texture3D.WSize = m_desc.depthOrArray;
                break;
            case Wild::TextureType::TEXTURE_2D:
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                break;
            case Wild::TextureType::TEXTURE_1D:
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                break;
            default:
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                break;
            }

            uavDesc.Texture2D.MipSlice = 0;

            if (m_desc.depthOrArray > 1 && m_desc.type != TextureType::TEXTURE_3D)
            {
                for (uint32_t mip = 0; mip < m_desc.mips; mip++)
                {
                    for (uint32_t face = 0; face < m_desc.depthOrArray; face++)
                    {
                        uavDesc.Texture2DArray.MipSlice = mip;
                        uavDesc.Texture2DArray.FirstArraySlice = face;
                        uavDesc.Texture2DArray.ArraySize = 1;
                        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                        m_uavArray.emplace_back(std::make_shared<UnorderedAccessView>(m_resource->Handle(), uavDesc));
                    }
                }

                m_uavArrayAvailiable = true;
            }
            else
            {
                m_uav = std::make_shared<UnorderedAccessView>(m_resource->Handle(), uavDesc);
            }
        }

        if (m_desc.flag & TextureDesc::ViewFlag::shaderResource)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = m_desc.format;

            switch (m_desc.type)
            {
            case Wild::TextureType::TEXTURE_3D:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                srvDesc.Texture3D.MipLevels = m_desc.mips;
                break;
            case Wild::TextureType::TEXTURE_2D:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = m_desc.mips;
                srvDesc.Texture2D.MostDetailedMip = 0;
                break;
            case Wild::TextureType::TEXTURE_1D:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                break;
            case Wild::TextureType::CUBEMAP:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                srvDesc.Texture2D.MipLevels = m_desc.mips;
                srvDesc.Texture2D.MostDetailedMip = 0;
                break;
            default:
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = m_desc.mips;
                srvDesc.Texture2D.MostDetailedMip = 0;
                break;
            }

            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            m_srv = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
        }

        m_resource->Handle()->SetName(StringToWString(m_desc.name).c_str());
    }

    Texture::Texture(const TextureDesc& desc, ComPtr<ID3D12Resource> resource, D3D12_RESOURCE_STATES initialState)
    {
        m_desc = desc;
        m_resource = std::make_unique<Resource>(initialState);
        m_resource->SetResource(resource);

        if (desc.flag & TextureDesc::renderTarget)
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            m_rtv = std::make_shared<RenderTargetView>(m_resource->Handle(), rtvDesc);
        }

        if (desc.flag & TextureDesc::depthStencil)
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

            m_dsv = std::make_shared<DepthStencilView>(m_resource->Handle(), dsvDesc);
        }
    }

    void Texture::Transition(CommandList& list, D3D12_RESOURCE_STATES newState) { m_resource->Transition(list, newState); }

    void Texture::CopyToDisk(const std::string& savePath)
    {
        auto& device = engine.GetGfxContext()->GetDevice();

        UINT numSubresources = m_desc.depthOrArray * m_desc.mips;
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(numSubresources);
        std::vector<UINT> numRows(numSubresources);
        std::vector<UINT64> rowSizes(numSubresources);
        UINT64 totalSize = 0;

        D3D12_RESOURCE_DESC texDesc = m_resource->GetDesc();
        device->GetCopyableFootprints(
            &texDesc, 0, numSubresources, 0, footprints.data(), numRows.data(), rowSizes.data(), &totalSize);

        // Readback buffer for fetching my texture data
        CD3DX12_HEAP_PROPERTIES readbackHeap(D3D12_HEAP_TYPE_READBACK);
        CD3DX12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSize);

        ComPtr<ID3D12Resource> readbackBuffer;
        device->CreateCommittedResource(&readbackHeap,
                                        D3D12_HEAP_FLAG_NONE,
                                        &bufDesc,
                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                        nullptr,
                                        IID_PPV_ARGS(&readbackBuffer));

        CommandList tempList(D3D12_COMMAND_LIST_TYPE_DIRECT);
        Transition(tempList, D3D12_RESOURCE_STATE_COPY_SOURCE);

        for (UINT sub = 0; sub < numSubresources; ++sub)
        {
            CD3DX12_TEXTURE_COPY_LOCATION dst(readbackBuffer.Get(), footprints[sub]);
            CD3DX12_TEXTURE_COPY_LOCATION src(m_resource->Handle().Get(), sub);
            tempList.GetList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }

        Transition(tempList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        tempList.Close();
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->ExecuteList(tempList);
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->WaitForFence();

        void* mappedData = nullptr;
        readbackBuffer->Map(0, nullptr, &mappedData);
        uint8_t* base = reinterpret_cast<uint8_t*>(mappedData);

        for (UINT mip = 0; mip < m_desc.mips; ++mip)
        {
            for (UINT face = 0; face < m_desc.depthOrArray; ++face)
            {
                UINT sub = mip + face * m_desc.mips;
                auto& fp = footprints[sub];
                UINT w = fp.Footprint.Width;
                UINT h = fp.Footprint.Height;
                UINT rowPitch = fp.Footprint.RowPitch;

                // 4 channel storage
                std::vector<float> pixels(w * h * 4);

                uint8_t* subBase = base + fp.Offset;
                for (UINT row = 0; row < h; ++row)
                {
                    uint16_t* src = reinterpret_cast<uint16_t*>(subBase + row * rowPitch);
                    float* dst = pixels.data() + row * w * 4;
                    for (UINT p = 0; p < w * 4; ++p)
                        dst[p] = f16Tof32(src[p]);
                }

                std::stringstream stringFormat{};
                stringFormat << savePath << "Mip" << mip << "/" << arr[face];
                std::filesystem::create_directories(savePath + "Mip" + std::to_string(mip));
                stbi_write_hdr(stringFormat.str().c_str(), w, h, 4, pixels.data());

                WD_INFO("Texuture successfuly loaded to disk: " + stringFormat.str());
            }
        }

        D3D12_RANGE emptyRange{0, 0};
        readbackBuffer->Unmap(0, &emptyRange);
    }

    bool Texture::LoadFromDisk(const std::string& loadPath)
    {
        auto& device = engine.GetGfxContext()->GetDevice();

        std::stringstream fileExistCheck{};
        fileExistCheck << loadPath << "Mip" << 0 << "/" << arr[0];

        if (!std::filesystem::exists(fileExistCheck.str()))
        {
            WD_INFO("File: " + loadPath + " is not cached yet.");
            return false;
        }

        CommandList tempList(D3D12_COMMAND_LIST_TYPE_DIRECT);
        Transition(tempList, D3D12_RESOURCE_STATE_COPY_DEST);

        UINT numSubresources = m_desc.depthOrArray * m_desc.mips;
        std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(numSubresources);
        std::vector<UINT> numRows(numSubresources);
        std::vector<UINT64> rowSizes(numSubresources);
        UINT64 totalSize = 0;
        D3D12_RESOURCE_DESC texDesc = m_resource->GetDesc();
        device->GetCopyableFootprints(
            &texDesc, 0, numSubresources, 0, footprints.data(), numRows.data(), rowSizes.data(), &totalSize);

        // One upload buffer for all subresources
        CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(totalSize);
        ComPtr<ID3D12Resource> uploadBuffer;
        device->CreateCommittedResource(&uploadHeap,
                                        D3D12_HEAP_FLAG_NONE,
                                        &bufferDesc,
                                        D3D12_RESOURCE_STATE_GENERIC_READ,
                                        nullptr,
                                        IID_PPV_ARGS(&uploadBuffer));

        void* mappedData = nullptr;
        uploadBuffer->Map(0, nullptr, &mappedData);
        uint8_t* base = reinterpret_cast<uint8_t*>(mappedData);

        for (UINT mip = 0; mip < m_desc.mips; ++mip)
        {
            for (UINT face = 0; face < m_desc.depthOrArray; ++face)
            {
                UINT sub = mip + face * m_desc.mips;
                auto& fp = footprints[sub];
                UINT w = fp.Footprint.Width;
                UINT h = fp.Footprint.Height;
                UINT rowPitch = fp.Footprint.RowPitch;

                std::stringstream path{};
                path << loadPath << "Mip" << mip << "/" << arr[face];

                int fileWidth{}, fileHeight{}, channels{};
                float* pixels = stbi_loadf(path.str().c_str(), &fileWidth, &fileHeight, &channels, 4);
                if (!pixels || fileWidth != (int)w || fileHeight != (int)h)
                {
                    WD_WARN(("Failed to load: " + loadPath + " texture was mot cached." + "\n").c_str());
                    return false;
                }

                // Copy into upload buffer respecting GPU row pitch alignment
                uint8_t* subBase = base + fp.Offset;
                for (UINT row = 0; row < h; ++row)
                {
                    float* src = pixels + row * w * 4;
                    uint16_t* dst = reinterpret_cast<uint16_t*>(subBase + row * rowPitch);
                    for (UINT p = 0; p < w * 4; ++p)
                        dst[p] = f32Tof16(src[p]);
                }

                stbi_image_free(pixels);

                CD3DX12_TEXTURE_COPY_LOCATION srcLoc(uploadBuffer.Get(), fp);
                CD3DX12_TEXTURE_COPY_LOCATION dstLoc(m_resource->Handle().Get(), sub);
                tempList.GetList()->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
            }
        }

        D3D12_RANGE emptyRange{0, 0};
        uploadBuffer->Unmap(0, &emptyRange);

        Transition(tempList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        tempList.Close();
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->ExecuteList(tempList);
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->WaitForFence();

        m_loadedFromDisk = true;
        return true;
    }

    /*void Texture::Barrier(CommandList& list, BarrierType type)
    {
        CD3DX12_RESOURCE_BARRIER resourceBarrier{};

        switch (type)
        {
        case BarrierType::none:
            break;
        case BarrierType::uav:
            resourceBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_resource->Handle().Get());
            break;
        default:
            break;
        }

        list.GetList()->ResourceBarrier(1, &resourceBarrier);
    }*/

    std::shared_ptr<RenderTargetView> Texture::GetRtv() const
    {
        if (!m_rtv)
        {
            WD_ERROR("rtv not availiable on this texture.");
            return nullptr;
        }

        return m_rtv;
    }

    std::shared_ptr<RenderTargetView> Texture::GetRtv(uint32_t index) const
    {
        if (!m_rtvArray[index])
        {
            std::string err = "Render target view with index: " + std::to_string(index) + " not availiable on texture: ";
            WD_ERROR(err.c_str(), m_desc.name.c_str());
            return nullptr;
        }

        return m_rtvArray[index];
    }

    std::shared_ptr<DepthStencilView> Texture::GetDsv() const
    {
        if (!m_dsv)
        {
            WD_ERROR("dsv not availiable on texture: ", m_desc.name.c_str());
            return nullptr;
        }

        return m_dsv;
    }

    std::shared_ptr<ShaderResourceView> Texture::GetSrv() const
    {
        if (!m_srv)
        {
            WD_ERROR("srv not availiable on texture: ", m_desc.name.c_str());
            return nullptr;
        }

        return m_srv;
    }

    std::shared_ptr<UnorderedAccessView> Texture::GetUav() const
    {
        if (!m_uav)
        {
            WD_ERROR("uav not availiable on texture:", m_desc.name.c_str());
            return nullptr;
        }

        return m_uav;
    }

    std::shared_ptr<UnorderedAccessView> Texture::GetUav(uint32_t index) const
    {
        if (!m_uavArray[index])
        {
            std::string err = "Unordered access view with index: " + std::to_string(index) + " not availiable on texture: ";
            WD_ERROR(err.c_str(), m_desc.name.c_str());
            return nullptr;
        }

        return m_uavArray[index];
    }

    void Texture::CreateTexture(const std::string& filePath)
    {
        auto& gfxContext = engine.GetGfxContext();

        int width, height, channels;
        stbi_uc* pixels = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        uint32_t imageSize = width * height * channels;

        if (!pixels)
        {
            WD_ERROR("No file exists at the file path.");
            return;
        }

        m_desc.width = static_cast<uint32_t>(width);
        m_desc.height = static_cast<uint32_t>(height);

        D3D12_RESOURCE_DESC textureDesc{};

        textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_desc.format,
                                                   static_cast<UINT64>(m_desc.width),
                                                   static_cast<UINT>(m_desc.height),
                                                   static_cast<UINT16>(m_desc.depthOrArray),
                                                   static_cast<UINT16>(m_desc.mips));

        // Create resource with copy dest since update subresource will transition it to that
        m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_COPY_DEST);

        ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                                       D3D12_HEAP_FLAG_NONE,
                                                                       &textureDesc,
                                                                       D3D12_RESOURCE_STATE_COMMON,
                                                                       nullptr,
                                                                       IID_PPV_ARGS(&m_resource->Handle())));

        // Create Upload heap
        UINT64 uploadBufferSize = GetRequiredIntermediateSize(GetResource(), 0, 1);

        ComPtr<ID3D12Resource> uploadHeap;
        ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                                       D3D12_HEAP_FLAG_NONE,
                                                                       &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                                                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                       nullptr,
                                                                       IID_PPV_ARGS(&uploadHeap)));

        D3D12_SUBRESOURCE_DATA subrsData{};
        subrsData.pData = pixels;
        subrsData.RowPitch = m_desc.width * 4;
        subrsData.SlicePitch = subrsData.RowPitch * m_desc.height;

        CommandList list(D3D12_COMMAND_LIST_TYPE_DIRECT);

        UpdateSubresources(list.GetList().Get(), GetResource(), uploadHeap.Get(), 0, 0, 1, &subrsData);

        m_resource->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        list.Close();
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->WaitForFence();

        stbi_image_free(pixels);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = m_desc.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        m_srv = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
    }

    void Texture::CreateCubeMapTexture(const std::string& filePath) {}

    void Texture::CreateSkyboxTexture(const std::string& filePath)
    {
        auto& gfxContext = engine.GetGfxContext();

        int width, height, channels;
        float* pixels = stbi_loadf(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (!pixels)
        {
            WD_ERROR("No data could be loaded.");
            return;
        }

        m_desc.width = static_cast<uint32_t>(width);
        m_desc.height = static_cast<uint32_t>(height);

        D3D12_RESOURCE_DESC textureDesc{};

        textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_desc.format,
                                                   static_cast<UINT64>(m_desc.width),
                                                   static_cast<UINT>(m_desc.height),
                                                   static_cast<UINT16>(m_desc.depthOrArray),
                                                   static_cast<UINT16>(m_desc.mips));

        // Create resource with copy dest since update subresource will transition it to that
        m_resource = std::make_unique<Resource>(D3D12_RESOURCE_STATE_COPY_DEST);

        ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                                       D3D12_HEAP_FLAG_NONE,
                                                                       &textureDesc,
                                                                       D3D12_RESOURCE_STATE_COMMON,
                                                                       nullptr,
                                                                       IID_PPV_ARGS(&m_resource->Handle())));

        // Create Upload heap
        UINT64 uploadBufferSize = GetRequiredIntermediateSize(GetResource(), 0, 1);

        ComPtr<ID3D12Resource> uploadHeap;
        ThrowIfFailed(gfxContext->GetDevice()->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                                       D3D12_HEAP_FLAG_NONE,
                                                                       &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
                                                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                                                       nullptr,
                                                                       IID_PPV_ARGS(&uploadHeap)));

        D3D12_SUBRESOURCE_DATA subrsData{};
        subrsData.pData = pixels;
        subrsData.RowPitch = m_desc.width * 4 * sizeof(float);
        subrsData.SlicePitch = subrsData.RowPitch * m_desc.height;

        CommandList list(D3D12_COMMAND_LIST_TYPE_DIRECT);

        UpdateSubresources(list.GetList().Get(), GetResource(), uploadHeap.Get(), 0, 0, 1, &subrsData);

        m_resource->Transition(list, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        list.Close();
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->ExecuteList(list);
        engine.GetGfxContext()->GetCommandQueue(QueueType::Direct)->WaitForFence();

        stbi_image_free(pixels);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = m_desc.format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        m_srv = std::make_shared<ShaderResourceView>(m_resource->Handle(), srvDesc);
    }

    float Texture::f16Tof32(uint16_t h)
    {

        uint32_t sign = (h >> 15) & 0x1;
        uint32_t exponent = (h >> 10) & 0x1f;
        uint32_t mantissa = h & 0x3ff;

        uint32_t f;
        if (exponent == 0)
            f = (sign << 31) | (mantissa << 13); // denorm
        else if (exponent == 31)
            f = (sign << 31) | (0xFF << 23) | (mantissa << 13); // inf/nan
        else
            f = (sign << 31) | ((exponent + 112) << 23) | (mantissa << 13);

        float result;
        memcpy(&result, &f, sizeof(float));
        return result;
    }

    uint16_t Texture::f32Tof16(float f)
    {
        uint32_t x;
        memcpy(&x, &f, sizeof(float));
        uint16_t sign = (x >> 16) & 0x8000;
        int32_t exponent = ((x >> 23) & 0xFF) - 127 + 15;
        uint32_t mantissa = x & 0x7FFFFF;
        if (exponent <= 0) return sign;
        if (exponent >= 31) return sign | 0x7C00;
        return sign | (exponent << 10) | (mantissa >> 13);
    }
} // namespace Wild
