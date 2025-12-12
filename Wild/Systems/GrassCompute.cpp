#include "Systems/GrassCompute.hpp"

namespace Wild {
	GrassCompute::GrassCompute() {
		m_computeShader = std::make_shared<Shader>("Shaders/computeGrassDataShader.hlsl");

		m_settings.ShaderState.ComputeShader = m_computeShader;

		std::vector<Uniform> uniforms;

		{
			Uniform uni{ 0, 0, RootParams::RootResourceType::UnorderedAccessView, sizeof(GrassBladeData) * MAXGRASSBLADES };
			uniforms.emplace_back(uni);
		}

		{
			Uniform uni{ 0, 0, RootParams::RootResourceType::Constants, sizeof(GrassComputeRC) };
			uniforms.emplace_back(uni);
		}

		m_pipeline = std::make_shared<PipelineState>(PipelineStateType::Compute, m_settings, uniforms);

		BufferDesc desc{};
		desc.bufferSize = sizeof(GrassBladeData);
		m_bladeDataBuffer = std::make_shared<Buffer>(desc);
		m_bladeDataBuffer->CreateUAVBuffer(MAXGRASSBLADES);
	}

	void GrassCompute::Render(CommandList& list)
	{
		list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_bladeDataBuffer->GetBuffer().Get(),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		));

		list.GetList()->SetPipelineState(m_pipeline->GetPso().Get());
		list.GetList()->SetComputeRootSignature(m_pipeline->GetRootSignature().Get());

		ID3D12DescriptorHeap* heaps[] = { engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get() };
		list.GetList()->SetDescriptorHeaps(1, heaps);

		list.GetList()->SetComputeRootUnorderedAccessView(
			0,
			m_bladeDataBuffer->GetBuffer()->GetGPUVirtualAddress()
		);

		list.GetList()->SetComputeRoot32BitConstants(
			1,
			sizeof(GrassComputeRC) / 4,
			&m_rc,
			0
		);

		// Were using 64 threads
		list.GetList()->Dispatch(MAXGRASSBLADES / 64, 1, 1);

		list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_bladeDataBuffer->GetBuffer().Get()));

		list.GetList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
			m_bladeDataBuffer->GetBuffer().Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		));
	}
}