#pragma once

#include "Renderer/CommandList.hpp"
#include "Renderer/CommandQueue.hpp"
#include "Renderer/Resources/Texture.hpp"

#include "Tools/DescriptorAllocator.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

namespace Wild {
	constexpr int BACK_BUFFER_COUNT = 2;

	class Device
	{
	public:
		Device(std::shared_ptr<Window> window);
		~Device() {};

		void Initialize();
		void Shutdown();

		ComPtr<ID3D12Device> GetDevice() { return m_device; }
		ComPtr<IDXGIFactory4> GetFactory() { return m_factory; }

		std::shared_ptr<CommandQueue> GetCommandQueue(QueueType type) { return m_commandQueue[static_cast<uint32_t>(type)]; }

		UINT GetBackBufferIndex() { return m_swapchain->GetCurrentBackBufferIndex(); }
		
		std::shared_ptr<Texture> GetRenderTarget() { return m_renderTargets[m_backBufferIndex]; }
		std::shared_ptr<Texture> GetDepthTarget() { return m_depthTarget; }

		std::shared_ptr<CommandList> GetCommandList() { return m_directCommandList[m_backBufferIndex]; }
		std::shared_ptr<CommandList> GetComputeCommandList() { return m_computeCommandList[m_backBufferIndex]; }

		void BeginFrame();
		void EndFrame();

		void ResizeWindow();

		int GetWidth() { return m_clientWidth; }
		int GetHeight() { return m_clientHeight; }

		void Flush();

		std::shared_ptr<DescriptorAllocatorRtv> GetRtvAllocator() { return m_descriptorAllocatorsRtv; }
		std::shared_ptr<DescriptorAllocatorDsv> GetDsvAllocator() { return m_descriptorAllocatorsDsv; }
		std::shared_ptr<DescriptorAllocatorCbvSrvUav> GetCbvSrvUavAllocator() { return m_desciptorAllocatorCbvSrvUav; }
	private:
		void SetupFactory();
		void CreateAdapter();
		void CreateDevice();
		void CreateSwapchain();

		void CreateTextureFromSwapchain(UINT index);

		ComPtr<IDXGIFactory4> m_factory;

		// factory flags
		UINT m_dxgiFactoryFlags = 0;

		ComPtr<ID3D12Debug1> m_debugController;

		ComPtr<IDXGIAdapter1> m_adapter;

		ComPtr<ID3D12Device> m_device;
		ComPtr<ID3D12DebugDevice> m_debugDevice;

		// Swapchain
		ComPtr<IDXGISwapChain3> m_swapchain;
		D3D12_VIEWPORT m_viewport;
		D3D12_RECT m_surfaceSize;

		std::shared_ptr<CommandList> m_directCommandList[BACK_BUFFER_COUNT];
		std::shared_ptr<CommandList> m_computeCommandList[BACK_BUFFER_COUNT];

		std::shared_ptr<Texture> m_renderTargets[BACK_BUFFER_COUNT];
		std::shared_ptr<Texture> m_depthTarget;

		// TODO make command queue for each type
		std::shared_ptr<CommandQueue> m_commandQueue[static_cast<uint32_t>(QueueType::Max)];

		std::shared_ptr<Window> m_window;

		std::shared_ptr<DescriptorAllocatorRtv> m_descriptorAllocatorsRtv;
		std::shared_ptr<DescriptorAllocatorDsv> m_descriptorAllocatorsDsv;
		std::shared_ptr<DescriptorAllocatorCbvSrvUav> m_desciptorAllocatorCbvSrvUav;

		UINT m_backBufferIndex{};
		UINT m_currentFrame{};

		bool m_vsyncEnabled = true;
		bool m_hdrEnabled = false;

		int m_clientWidth{}, m_clientHeight{};
	};
}