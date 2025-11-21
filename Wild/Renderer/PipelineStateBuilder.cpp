#include "Renderer/PipelineStateBuilder.hpp"

#include "Tools/StateTransfer.hpp"

namespace Wild {
	PipelineState::PipelineState(PipelineStateType Type, const PipelineStateSettings& settings, const std::vector<Uniform>& uniforms) : m_settings(settings)
	{
        m_type = Type;

        CreateRootSignature(uniforms);

        if (!m_rootSignature)
            WD_FATAL("No root signature supplied in pipelineSettings.");

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
        auto device = engine.GetDevice();

        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(device->GetDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;


        std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters{};
        rootParameters.resize(uniforms.size());

        for (size_t i = 0; i < rootParameters.size(); i++)
        {
            const Uniform& uniform = uniforms[i];

            switch (uniform.Type)
            {
            case RootParams::Constants:
                if (!m_hasRootConstant) {
                    if (uniform.ResourceSize == 0)
                        WD_FATAL("No resource size supplied for root constant.");

                    rootParameters[i].InitAsConstants((uniform.ResourceSize / 4), uniform.ShaderRegister, uniform.RegisterSpace);
                    m_hasRootConstant = true;
                }
                else
                    WD_WARN("Tried to initialize more than 1 root constant in the same pass.");
                break;
            case RootParams::ConstantBufferView:
                rootParameters[i].InitAsConstantBufferView(uniform.ShaderRegister, uniform.RegisterSpace, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, uniform.Visibility);
                break;
            case RootParams::ShaderResourceView:
                rootParameters[i].InitAsShaderResourceView(uniform.ShaderRegister, uniform.RegisterSpace, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, uniform.Visibility);
                break;
            case RootParams::UnorderedAccessView:
                rootParameters[i].InitAsUnorderedAccessView(uniform.ShaderRegister, uniform.RegisterSpace, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, uniform.Visibility);
                break;
            case RootParams::DescriptorTable:
                //rootParameters[i].InitAsDescriptorTable();
                WD_WARN("To be implemented.");
                break;
            default:
                break;
            }
        }

        // Root signature layout
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        rootSignatureDesc.Desc_1_1.NumParameters = 1;
        rootSignatureDesc.Desc_1_1.pParameters = rootParameters.data();
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;

        ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
        ThrowIfFailed(device->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        m_rootSignature->SetName(L"Wild Root Signature");

        if (error)
        {
            const char* errStr = (const char*)error->GetBufferPointer();
            WD_ERROR(errStr);
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
        psoDesc.pRootSignature = m_rootSignature.Get();

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

    void PipelineState::CreateComputePSO()
    {
        auto device = engine.GetDevice();
        if (m_settings.ShaderState.ComputeShader)
        {
            WD_WARN("No compute shader supplied in compute pipeline");
            return;
        }

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.CS = m_settings.ShaderState.ComputeShader->GetByteCode();

        ThrowIfFailed(device->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
    }
}