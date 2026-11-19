#pragma once

#include "Renderer/CommandList.hpp"
#include "Renderer/CommandQueue.hpp"
#include "Renderer/PerFrame.hpp"
#include "Renderer/Resources/Texture.hpp"
#include "Renderer/Window.hpp"

#include "Tools/BufferAllocator.hpp"
#include "Tools/DescriptorAllocator.hpp"
#include "Tools/DeviceCapabilities.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <D3D12MemAlloc.h>

#include <deque>
#include <vector>

namespace Wild
{
    class GfxContext
    {
      public:
        GfxContext(std::shared_ptr<Window> window);
        ~GfxContext() {};

        void Initialize();
        void Shutdown();

        ComPtr<ID3D12Device> GetDevice() { return m_device; }
        ComPtr<ID3D12Device2> GetDevice2() { return m_device2; }
        ComPtr<ID3D12Device7> GetDevice7() { return m_device7; }
        ComPtr<IDXGIFactory4> GetFactory() { return m_factory; }
        ComPtr<D3D12MA::Allocator> GetAllocator() { return m_allocator; }
        ComPtr<ID3D12PipelineLibrary> GetPipelineLibrary() { return m_pipelineLibrary; }
        std::string GetAdapterName();

        const GfxCapabilities& GetCapabilities() const { return m_capabilities; }

        // Get queue from input variable type
        std::shared_ptr<CommandQueue> GetCommandQueue(QueueType type) { return m_commandQueue[static_cast<uint32_t>(type)]; }

        // Get frame index
        UINT GetBackBufferIndex() { return m_swapchain->GetCurrentBackBufferIndex(); }

        std::shared_ptr<Texture> GetRenderTarget() { return m_renderTargets[m_backBufferIndex]; }
        std::shared_ptr<Texture> GetDepthTarget() { return m_depthTarget; }

        std::shared_ptr<CommandList> GetCommandList() { return m_directCommandList[m_backBufferIndex]; }

        void CachePipelineLibrary();

        // TODO add command list for assync compute commands
        // std::shared_ptr<CommandList> GetComputeCommandList() { return m_computeCommandList[m_backBufferIndex]; }

        void BeginFrame();
        void EndFrame();

        bool ResizeWindow();

        int GetWidth() const { return glm::max(m_clientWidth, 1); }
        int GetHeight() const { return glm::max(m_clientHeight, 1); }

       void Flush();

        void DeferRelease(ComPtr<ID3D12Resource> resource, ComPtr<D3D12MA::Allocation> allocation);

        void DeferDescriptorFree(DescriptorAllocator* allocator, uint32_t handle);

        bool IsDeferringReleases() const { return m_deferReleases; }

        std::shared_ptr<DescriptorAllocatorRtv> GetRtvAllocator() { return m_descriptorAllocatorsRtv; }
        std::shared_ptr<DescriptorAllocatorDsv> GetDsvAllocator() { return m_descriptorAllocatorsDsv; }
        std::shared_ptr<DescriptorAllocatorCbvSrvUav> GetCbvSrvUavAllocator() { return m_desciptorAllocatorCbvSrvUav; }
        std::shared_ptr<BufferAllocator> GetBufferAllocator() { return m_bufferAllocator; }

      private:
        // Entrypoint to graphics API
        void SetupFactory();

        // Enemurate over all the physical graphics adapters and pick one that supports the feature level
        void CreateAdapter();

        // Creating the actual device object
        void CreateDevice();

        // Creates the D3D12 Memory Allocator used to back all buffer and texture allocations
        void CreateAllocator();

        // Creates or resizes swapchain if availiable
        void CreateSwapchain();
        void CreateTextureFromSwapchain(UINT index);

        void CreatePipelineLibrary();
        bool m_usesLegacyCache = false;
        std::vector<uint8_t> m_libraryData;

        ComPtr<IDXGIFactory4> m_factory;

        // factory flags
        UINT m_dxgiFactoryFlags = 0;

        // DirectX 12 resources
        ComPtr<ID3D12Debug1> m_debugController;
        ComPtr<IDXGIAdapter1> m_adapter;
        ComPtr<ID3D12Device> m_device;
        ComPtr<ID3D12Device2> m_device2;
        ComPtr<ID3D12Device7> m_device7;
        ComPtr<ID3D12DebugDevice> m_debugDevice;

        // D3D12 Memory Allocator instance
        ComPtr<D3D12MA::Allocator> m_allocator;

        // Pipeline library for caching the PSO's
        ComPtr<ID3D12PipelineLibrary> m_pipelineLibrary;

        GfxCapabilities m_capabilities;

        // Swapchain
        ComPtr<IDXGISwapChain3> m_swapchain;
        D3D12_VIEWPORT m_viewport;
        D3D12_RECT m_surfaceSize;

        std::shared_ptr<CommandList> m_directCommandList[BACK_BUFFER_COUNT];

        // Output render targets
        std::shared_ptr<Texture> m_renderTargets[BACK_BUFFER_COUNT];
        std::shared_ptr<Texture> m_depthTarget;

        std::shared_ptr<CommandQueue> m_commandQueue[static_cast<uint32_t>(QueueType::Max)];

        std::shared_ptr<Window> m_window;

        // Resource allocators
        std::shared_ptr<DescriptorAllocatorRtv> m_descriptorAllocatorsRtv;
        std::shared_ptr<DescriptorAllocatorDsv> m_descriptorAllocatorsDsv;
        std::shared_ptr<DescriptorAllocatorCbvSrvUav> m_desciptorAllocatorCbvSrvUav;

        // Creates and owns all buffer resource allocations through the D3D12 memory allocator
        std::shared_ptr<BufferAllocator> m_bufferAllocator;

        UINT m_backBufferIndex{};
        UINT m_currentFrame{};

        uint64_t m_frameFenceValues[BACK_BUFFER_COUNT]{};

        // True between BeginFrame and EndFrame, while the frame's command list is open
        bool m_frameOpen = false;

        // Deferred release queue
        struct DeferredRelease
        {
            ComPtr<ID3D12Resource> resource;
            ComPtr<D3D12MA::Allocation> allocation;
            DescriptorAllocator* descriptorAllocator = nullptr;
            uint32_t descriptorHandle = 0;
        };

        struct DeferredReleaseBatch
        {
            uint64_t fenceValue = 0; // Safe to release once the direct queue fence reaches this value
            std::vector<DeferredRelease> releases;
        };

        // Frees since the last EndFrame, they get the fence value of the frame that is submitted next
        std::vector<DeferredRelease> m_pendingReleases;

        // Frees tagged with a fence value, oldest first
        std::deque<DeferredReleaseBatch> m_queuedReleases;

        bool m_deferReleases = false;

        // Releases every queued batch whose fence value the GPU has completed
        void ProcessDeferredReleases();
        static void Release(std::vector<DeferredRelease>& releases);

        bool m_vsyncEnabled = true;
        bool m_hdrEnabled = false;

        int m_clientWidth{}, m_clientHeight{};
    };
} // namespace Wild
