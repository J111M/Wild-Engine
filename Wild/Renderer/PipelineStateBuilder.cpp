#include "Renderer/PipelineStateBuilder.hpp"

#include "Tools/StateTransfer.hpp"

namespace Wild {
	PipelineState::PipelineState(PipelineStateType Type, const PipelineStateSettings& settings) : m_settings(settings)
	{
        m_type = Type;

        if (!m_settings.RootSignature)
            WD_FATAL("No root signature supplied in pipelineSettings.");

        switch (m_type)
        {
        case Wild::PipelineStateType::Graphics:
            CreateGraphicsPSO();
            break;
        case Wild::PipelineStateType::Compute:
            break;
        default:
            break;
        }   
	}

    void PipelineState::CreateGraphicsPSO()
    {
        auto device = engine.GetDevice();
        
        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(glm::vec3), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(glm::vec3) * 2, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, sizeof(glm::vec3) * 3, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.pRootSignature = m_settings.RootSignature.Get();

        if (m_settings.ShaderState.VertexShader)
            psoDesc.VS = m_settings.ShaderState.VertexShader->GetByteCode();

        if (m_settings.ShaderState.FragShader)
            psoDesc.PS = m_settings.ShaderState.FragShader->GetByteCode();

        psoDesc.PrimitiveTopologyType = GetTopologyMode(m_settings.RasterizerState.TopologyMode);
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        psoDesc.SampleMask = 0xffffffff;

        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = GetWindingOrder(m_settings.RasterizerState.WindingMode);
        psoDesc.RasterizerState.CullMode = GetCullMode(m_settings.RasterizerState.CullMode);
        psoDesc.RasterizerState.FillMode = GetFillMode(m_settings.RasterizerState.FillMode);

        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.BlendState.AlphaToCoverageEnable = m_settings.RasterizerState.BlendDesc.AlphaToCoverageEnable;
        psoDesc.BlendState.IndependentBlendEnable = m_settings.RasterizerState.BlendDesc.IndependentBlendEnable;

        // Set blend state for all possible render targets
        for (size_t i = 0; i < 8; i++)
        {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = m_settings.RasterizerState.BlendDesc.RenderTarget->BlendEnable;
            psoDesc.BlendState.RenderTarget[i].SrcBlend = GetBlendState(m_settings.RasterizerState.BlendDesc.RenderTarget->SrcBlend);
            psoDesc.BlendState.RenderTarget[i].DestBlend = GetBlendState(m_settings.RasterizerState.BlendDesc.RenderTarget->DestBlend);
            psoDesc.BlendState.RenderTarget[i].BlendOp = GetBlendOpState(m_settings.RasterizerState.BlendDesc.RenderTarget->BlendOperation);
            psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = GetBlendState(m_settings.RasterizerState.BlendDesc.RenderTarget->SrcBlendAlpha);
            psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = GetBlendState(m_settings.RasterizerState.BlendDesc.RenderTarget->DestBlendAlpha);
            psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = GetBlendOpState(m_settings.RasterizerState.BlendDesc.RenderTarget->BlendOperationAlpha);
        }

        psoDesc.NumRenderTargets = 1;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = m_settings.DepthStencilState.DepthEnable;
        psoDesc.DepthStencilState.DepthWriteMask = GetDepthWrite(m_settings.DepthStencilState.DepthWriteMask);
        psoDesc.DepthStencilState.DepthFunc = GetComparisonFunc(m_settings.DepthStencilState.DepthFunc);
        psoDesc.DepthStencilState.StencilEnable = m_settings.DepthStencilState.StencilEnable;
        psoDesc.DepthStencilState.StencilReadMask = m_settings.DepthStencilState.StencilReadMask;
        psoDesc.DepthStencilState.StencilWriteMask = m_settings.DepthStencilState.StencilWriteMask;

        // Front face stencil operations
        psoDesc.DepthStencilState.FrontFace.StencilFailOp = GetDepthStencilOp(m_settings.DepthStencilState.FrontFace.StencilFailOp);
        psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = GetDepthStencilOp(m_settings.DepthStencilState.FrontFace.StencilDepthFailOp);
        psoDesc.DepthStencilState.FrontFace.StencilPassOp = GetDepthStencilOp(m_settings.DepthStencilState.FrontFace.StencilPassOp);
        psoDesc.DepthStencilState.FrontFace.StencilFunc = GetComparisonFunc(m_settings.DepthStencilState.FrontFace.StencilFunc);

        // Back face stencil operations
        psoDesc.DepthStencilState.BackFace.StencilFailOp = GetDepthStencilOp(m_settings.DepthStencilState.BackFace.StencilFailOp);
        psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = GetDepthStencilOp(m_settings.DepthStencilState.BackFace.StencilDepthFailOp);
        psoDesc.DepthStencilState.BackFace.StencilPassOp = GetDepthStencilOp(m_settings.DepthStencilState.BackFace.StencilPassOp);
        psoDesc.DepthStencilState.BackFace.StencilFunc = GetComparisonFunc(m_settings.DepthStencilState.BackFace.StencilFunc);

        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        ThrowIfFailed(device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
    }
}