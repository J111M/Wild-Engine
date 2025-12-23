#include "Renderer/PipelineStateBuilder.hpp"
#include "Renderer/RootSignatureBuilder.hpp"

#include "Tools/StateTransfer.hpp"

namespace Wild {
	PipelineState::PipelineState(PipelineStateType Type, const PipelineStateSettings& settings, const std::vector<Uniform>& uniforms) : m_settings(settings)
	{
        m_type = Type;

        CreateRootSignature(uniforms);

        if (!m_rootSignature)
            WD_FATAL("Failed to create root signature.");

        switch (m_type)
        {
        case Wild::PipelineStateType::Graphics:
            CreateGraphicsPSO();
            break;
        case Wild::PipelineStateType::Compute:
            CreateComputePSO();
            break;
        default:
            break;
        }   
	}

    void PipelineState::CreateRootSignature(const std::vector<Uniform>& uniforms) {
        auto gfxContext = engine.GetGfxContext();

        RootSignatureBuilder builder;

        for (size_t i = 0; i < uniforms.size(); i++)
        {
            const Uniform& uniform = uniforms[i];

            switch (uniform.Type)
            {
            case RootParams::Constants:
                builder.AddConstants((uniform.ResourceSize / 4), uniform.ShaderRegister, uniform.RegisterSpace, uniform.Visibility);
                break;
            case RootParams::ConstantBufferView:
                builder.AddCBV(uniform.ShaderRegister, uniform.RegisterSpace, uniform.Visibility);
                break;
            case RootParams::ShaderResourceView:
                builder.AddSRV(uniform.ShaderRegister, uniform.RegisterSpace, uniform.Visibility);
                break;
            case RootParams::UnorderedAccessView:
                builder.AddUAV(uniform.ShaderRegister, uniform.RegisterSpace, uniform.Visibility);
                break;
            case RootParams::DescriptorTable:
                builder.AddDescriptorTable(uniform.Ranges, uniform.Visibility);
                break;
            case RootParams::StaticSampler:
                builder.AddStaticSampler(uniform.ShaderRegister, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, uniform.RegisterSpace, uniform.Visibility);
                break;
            }
        }

        m_rootSignature = builder.Build(gfxContext->GetDevice().Get());
    }

    void PipelineState::CreateGraphicsPSO()
    {
        auto gfxContext = engine.GetGfxContext();

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { m_settings.ShaderState.InputLayout.data(), (UINT)m_settings.ShaderState.InputLayout.size()};
        psoDesc.pRootSignature = m_rootSignature.Get();

        if (m_settings.ShaderState.VertexShader)
            psoDesc.VS = m_settings.ShaderState.VertexShader->GetByteCode();

        if (m_settings.ShaderState.FragShader)
            psoDesc.PS = m_settings.ShaderState.FragShader->GetByteCode();

        psoDesc.PrimitiveTopologyType = GetTopologyModeType(m_settings.RasterizerState.TopologyMode);

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
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        // TODO change dynamically to the amount of rendertargets
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
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

        ThrowIfFailed(gfxContext->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
    }

    void PipelineState::CreateComputePSO()
    {
        auto gfxContext = engine.GetGfxContext();
        if (!m_settings.ShaderState.ComputeShader)
        {
            WD_WARN("No compute shader supplied in compute pipeline");
            return;
        }

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.CS = m_settings.ShaderState.ComputeShader->GetByteCode();

        ThrowIfFailed(gfxContext->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
    }
}