#include "Renderer/PipelineStateBuilder.hpp"

namespace Wild {
	PipelineState::PipelineState(PipelineStateType Type, const PipelineStateSettings& settings)
	{
        //if (!pipelineSettings.RootSignature)
        //    WD_FATAL("No root signature supplied in pipelineSettings.");

        //D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        //psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        //psoDesc.pRootSignature = pipelineSettings.RootSignature.Get();

        //if (pipelineSettings.ShaderState.VertexShader)
        //    psoDesc.VS = pipelineSettings.ShaderState.VertexShader->GetByteCode();

        //if (pipelineSettings.ShaderState.FragShader)
        //    psoDesc.PS = pipelineSettings.ShaderState.FragShader->GetByteCode();



        //psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        //psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        ////psoDesc.SampleDesc = sampleDesc;
        //psoDesc.SampleMask = 0xffffffff;
        //psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        //psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
        //psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        //psoDesc.NumRenderTargets = 1;
        //psoDesc.SampleDesc.Count = 1;
        //psoDesc.SampleDesc.Quality = 0;

        //psoDesc.DepthStencilState.DepthEnable = TRUE;
        //psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        //psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

        //psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        //ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&psoIn)));
	}
}