#pragma once

#include "Tools/Common3d12.hpp"
#include "Tools/States.hpp"

namespace Wild
{
    // psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    // psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
    // psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    // psoDesc.NumRenderTargets = 1;
    // psoDesc.SampleDesc.Count = 1;
    // psoDesc.SampleDesc.Quality = 0;

    // psoDesc.DepthStencilState.DepthEnable = m_settings.DepthStencilState.DepthEnable;
    // psoDesc.DepthStencilState.DepthWriteMask =
    // static_cast<D3D12_DEPTH_WRITE_MASK>(m_settings.DepthStencilState.DepthWriteMask); psoDesc.DepthStencilState.DepthFunc =
    // static_cast<D3D12_COMPARISON_FUNC>(m_settings.DepthStencilState.DepthFunc);

    // D3D12_FILL_MODE FillMode;
    // D3D12_CULL_MODE CullMode;
    // BOOL FrontCounterClockwise;
    // INT DepthBias;
    // FLOAT DepthBiasClamp;
    // FLOAT SlopeScaledDepthBias;
    // BOOL DepthClipEnable;
    // BOOL MultisampleEnable;
    // BOOL AntialiasedLineEnable;
    // UINT ForcedSampleCount;
    // D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;

    D3D12_PRIMITIVE_TOPOLOGY GetTopologyMode(PrimitiveTopology primitive)
    {
        switch (primitive)
        {
        case Wild::PrimitiveTopology::Undefined:
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            break;
        case Wild::PrimitiveTopology::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case Wild::PrimitiveTopology::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case Wild::PrimitiveTopology::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case Wild::PrimitiveTopology::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case Wild::PrimitiveTopology::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        default:
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            break;
        }
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE GetTopologyModeType(PrimitiveTopology primitive)
    {
        switch (primitive)
        {
        case Wild::PrimitiveTopology::Undefined:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        case Wild::PrimitiveTopology::TriangleList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case Wild::PrimitiveTopology::TriangleStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case Wild::PrimitiveTopology::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case Wild::PrimitiveTopology::LineList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case Wild::PrimitiveTopology::LineStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        }
    }

    D3D12_FILL_MODE GetFillMode(FillMode fillMode)
    {
        switch (fillMode)
        {
        case Wild::FillMode::Wireframe:
            return D3D12_FILL_MODE_WIREFRAME;
        case Wild::FillMode::Solid:
            return D3D12_FILL_MODE_SOLID;
        default:
            return D3D12_FILL_MODE_SOLID;
        }
    }

    D3D12_CULL_MODE GetCullMode(CullMode cullMode)
    {
        switch (cullMode)
        {
        case Wild::CullMode::None:
            return D3D12_CULL_MODE_NONE;
        case Wild::CullMode::Front:
            return D3D12_CULL_MODE_FRONT;
        case Wild::CullMode::Back:
            return D3D12_CULL_MODE_BACK;
        default:
            return D3D12_CULL_MODE_NONE;
        }
    }

    BOOL GetWindingOrder(WindingOrder order)
    {
        switch (order)
        {
        case Wild::WindingOrder::Clockwise:
            return 0;
        case Wild::WindingOrder::CounterClockwise:
            return 1;
        default:
            return 0;
        }
    }

    D3D12_BLEND GetBlendState(Blend blend)
    {
        switch (blend)
        {
        case Wild::Blend::Zero:
            return D3D12_BLEND_ZERO;
        case Wild::Blend::One:
            return D3D12_BLEND_ONE;
        case Wild::Blend::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case Wild::Blend::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case Wild::Blend::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case Wild::Blend::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case Wild::Blend::DstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case Wild::Blend::InvDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case Wild::Blend::DstColor:
            return D3D12_BLEND_DEST_COLOR;
        case Wild::Blend::InvDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case Wild::Blend::SrcAlphaSat:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case Wild::Blend::BlendFactor:
            return D3D12_BLEND_BLEND_FACTOR;
        case Wild::Blend::InvBlendFactor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case Wild::Blend::Src1Color:
            return D3D12_BLEND_SRC1_COLOR;
        case Wild::Blend::InvSrc1Color:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case Wild::Blend::Src1Alpha:
            return D3D12_BLEND_SRC1_ALPHA;
        case Wild::Blend::InvSrc1Alpha:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default:
            return D3D12_BLEND_ZERO;
        }
    }

    D3D12_BLEND_OP GetBlendOpState(BlendOp blendOp)
    {
        switch (blendOp)
        {
        case Wild::BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case Wild::BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case Wild::BlendOp::RevSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case Wild::BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case Wild::BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
        }
    }

    D3D12_DEPTH_WRITE_MASK GetDepthWrite(DepthWriteMask depthWriteMask)
    {
        switch (depthWriteMask)
        {
        case Wild::DepthWriteMask::Zero:
            return D3D12_DEPTH_WRITE_MASK_ZERO;
        case Wild::DepthWriteMask::All:
            return D3D12_DEPTH_WRITE_MASK_ALL;
        default:
            return D3D12_DEPTH_WRITE_MASK_ZERO;
        }
    }

    D3D12_COMPARISON_FUNC GetComparisonFunc(ComparisonFunc depthFunc)
    {
        switch (depthFunc)
        {
        case Wild::ComparisonFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case Wild::ComparisonFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case Wild::ComparisonFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case Wild::ComparisonFunc::LessEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case Wild::ComparisonFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case Wild::ComparisonFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case Wild::ComparisonFunc::GreaterEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case Wild::ComparisonFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    D3D12_STENCIL_OP GetDepthStencilOp(StencilOp stencilOp)
    {
        switch (stencilOp)
        {
        case Wild::StencilOp::Keep:
            return D3D12_STENCIL_OP_KEEP;
        case Wild::StencilOp::Zero:
            return D3D12_STENCIL_OP_ZERO;
        case Wild::StencilOp::Replace:
            return D3D12_STENCIL_OP_REPLACE;
        case Wild::StencilOp::IncrSat:
            return D3D12_STENCIL_OP_INCR_SAT;
        case Wild::StencilOp::DecrSat:
            return D3D12_STENCIL_OP_DECR_SAT;
        case Wild::StencilOp::Invert:
            return D3D12_STENCIL_OP_INVERT;
        case Wild::StencilOp::Incr:
            return D3D12_STENCIL_OP_INCR;
        case Wild::StencilOp::Decr:
            return D3D12_STENCIL_OP_DECR;
        default:
            return D3D12_STENCIL_OP_KEEP;
        }
    }
} // namespace Wild
