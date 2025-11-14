#include "Renderer/Resources/Texture.hpp"

namespace Wild {
    Texture::Texture(const TextureDesc& desc)
    {
        m_desc = desc;

        auto device = engine.GetDevice();

        if (desc.flag & TextureDesc::renderTarget) {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            m_rtv = std::make_shared<RenderTargetView>(m_resource, rtvDesc);
        }

        if (desc.flag & TextureDesc::depthStencil) {
            D3D12_RESOURCE_DESC depthDesc = {};
            depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            depthDesc.Alignment = 0;
            depthDesc.Width = m_desc.width;
            depthDesc.Height = m_desc.height;
            depthDesc.DepthOrArraySize = 1;
            depthDesc.MipLevels = 1;
            depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
            depthDesc.SampleDesc.Count = 1;
            depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            clearValue.DepthStencil = { 1.0f, 0 };

            device->GetDevice()->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &depthDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clearValue,
                IID_PPV_ARGS(&m_resource)
            );

            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

            m_dsv = std::make_shared<DepthStencilView>(m_resource, dsvDesc);
        }
    }

    Texture::Texture(const TextureDesc& desc, ComPtr<ID3D12Resource2> resource)
    {
        m_desc = desc;
        m_resource = resource;

        if (desc.flag & TextureDesc::renderTarget) {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            m_rtv = std::make_shared<RenderTargetView>(m_resource, rtvDesc);
        }

        if (desc.flag & TextureDesc::depthStencil) {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

            m_dsv = std::make_shared<DepthStencilView>(m_resource, dsvDesc);
        }
    }

    std::shared_ptr<RenderTargetView> Texture::GetRtv() const
    {
        if (!m_rtv) {
            WD_ERROR("rtv not availiable on this texture.");
            return nullptr;
        }
           
        return m_rtv;
    }

    std::shared_ptr<DepthStencilView> Texture::GetDsv() const
    {
        if (!m_dsv) {
            WD_ERROR("dsv not availiable on this texture.");
            return nullptr;
        }

        return m_dsv;
    }
}