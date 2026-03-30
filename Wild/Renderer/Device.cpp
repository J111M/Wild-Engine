#include "Renderer/Device.hpp"

#include <codecvt>
#include <fstream>
#include <initguid.h>
#include <locale>

#include <dxgidebug.h>

namespace Wild
{
    // Prevent copy by using move
    GfxContext::GfxContext(std::shared_ptr<Window> window) : m_window(std::move(window))
    {
        m_clientWidth = m_window->GetWidth();
        m_clientHeight = m_window->GetHeight();

        m_surfaceSize.left = 0;
        m_surfaceSize.top = 0;
        m_surfaceSize.right = static_cast<LONG>(m_window->GetWidth());
        m_surfaceSize.bottom = static_cast<LONG>(m_window->GetHeight());

        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;
        m_viewport.Width = static_cast<float>(m_window->GetWidth());
        m_viewport.Height = static_cast<float>(m_window->GetHeight());
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;
    }

    void GfxContext::Shutdown()
    {
        // CachePipelineLibrary();

        // Release resources manually since we are working with static objects
        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            m_renderTargets[i].reset();
        }

        m_depthTarget.reset();

        m_desciptorAllocatorCbvSrvUav.reset();
        m_descriptorAllocatorsDsv.reset();
        m_descriptorAllocatorsRtv.reset();

#ifdef DEBUG
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        }
#endif
    }

    std::string GfxContext::GetAdapterName()
    {
        DXGI_ADAPTER_DESC1 desc;
        m_adapter->GetDesc1(&desc);

        return WStringToString(desc.Description);
    }

    void GfxContext::Initialize()
    {
        SetupFactory();
        CreateAdapter();
        CreateDevice();

        m_capabilities.GetSupportedFeatures(this);

        CreatePipelineLibrary();

        m_descriptorAllocatorsRtv = std::make_shared<DescriptorAllocatorRtv>(m_device, 64);
        m_descriptorAllocatorsDsv = std::make_shared<DescriptorAllocatorDsv>(m_device, 64);
        m_desciptorAllocatorCbvSrvUav = std::make_shared<DescriptorAllocatorCbvSrvUav>(m_device, 8192);

        m_commandQueue[static_cast<uint32_t>(QueueType::Direct)] =
            std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, "Direct queue");
        m_commandQueue[static_cast<uint32_t>(QueueType::Compute)] =
            std::make_shared<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_COMPUTE, "Direct queue");

        CreateSwapchain();

        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            m_directCommandList[i] = std::make_shared<CommandList>(D3D12_COMMAND_LIST_TYPE_DIRECT);
            // m_computeCommandList[i] = std::make_shared<CommandList>(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        }
    }

    void GfxContext::BeginFrame()
    {
        m_currentFrame = m_backBufferIndex;
        m_backBufferIndex = GetBackBufferIndex();

        auto currentCommandList = m_directCommandList[m_backBufferIndex];
        auto backBuffer = m_renderTargets[m_backBufferIndex];

        backBuffer->Transition(*currentCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        FLOAT clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};

        currentCommandList->GetList()->ClearRenderTargetView(backBuffer->GetRtv()->GetCpuHandle(), clearColor, 0, nullptr);
        currentCommandList->GetList()->ClearDepthStencilView(
            m_depthTarget->GetDsv()->GetCpuHandle(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    }

    void GfxContext::EndFrame()
    {
        auto currentCommandList = m_directCommandList[m_backBufferIndex];
        auto backBuffer = m_renderTargets[m_backBufferIndex];

        // Present frame
        backBuffer->Transition(*currentCommandList, D3D12_RESOURCE_STATE_PRESENT);

        ThrowIfFailed(currentCommandList->GetList()->Close());

        ID3D12CommandList* const commandLists[] = {currentCommandList->GetList().Get()};

        GetCommandQueue(QueueType::Direct)->GetQueue()->ExecuteCommandLists(_countof(commandLists), commandLists);
        GetCommandQueue(QueueType::Direct)->WaitForFence();

        uint32_t flags = DXGI_PRESENT_ALLOW_TEARING;
        UINT sync_interval = 0;

        if (m_vsyncEnabled)
        {
            flags = 0;
            sync_interval = 1;
        }

        ThrowIfFailed(m_swapchain->Present(sync_interval, flags));

        // Reset allocator and list
        currentCommandList->ResetList();
    }

    bool GfxContext::ResizeWindow()
    {
        int windowWidth = m_window->GetWidth();
        int windowHeight = m_window->GetHeight();

        if (m_clientWidth != windowWidth || m_clientHeight != windowHeight)
        {

            // We don't want the back buffer to be smaller than 1
            m_clientWidth = std::max(1, windowWidth);
            m_clientHeight = std::max(1, windowHeight);

            // Make sure queue is empty
            GetCommandQueue(QueueType::Direct)->WaitForFence();

            // Clear possible render targets
            for (int i = 0; i < BACK_BUFFER_COUNT; i++)
            {
                if (m_renderTargets[i]) m_renderTargets[i].reset();
            }

            if (m_depthTarget) m_depthTarget.reset();

            // Flush all command before recreating the swap chain
            Flush();

            // Resize or recreate swapchain if it doesn't exist
            CreateSwapchain();

            WD_INFO("Window is resized succsesfully!");

            // Set new client values
            m_clientWidth = m_window->GetWidth();
            m_clientHeight = m_window->GetHeight();

            return true;
        }

        return false;
    }

    void GfxContext::Flush() { GetCommandQueue(QueueType::Direct)->WaitForFence(); }

    void GfxContext::SetupFactory()
    {
#if defined(_DEBUG)

        ID3D12Debug* dc{};
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&dc)));
        ThrowIfFailed(dc->QueryInterface(IID_PPV_ARGS(&m_debugController)));
        m_debugController->EnableDebugLayer();
        m_debugController->SetEnableGPUBasedValidation(true);
        m_dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        dc->Release();
        dc = nullptr;
#endif
        ThrowIfFailed(CreateDXGIFactory2(m_dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));
    }

    void GfxContext::CreateAdapter()
    {
        for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(adapterIndex, &m_adapter); ++adapterIndex)
        {

            DXGI_ADAPTER_DESC1 desc;
            m_adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { continue; }

            if (D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device), nullptr))
            {
                std::string adapterDescription = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(desc.Description);

                std::string adapterInfo = "Adapter " + std::to_string(adapterIndex) + ": " + adapterDescription;
                std::string vramInfo = "VRAM: " + std::to_string(desc.DedicatedVideoMemory / (1024 * 1024)) + " MB";
                std::string vendorInfo =
                    "VendorId: " + std::to_string(desc.VendorId) + " DeviceId: " + std::to_string(desc.DeviceId);

                WD_INFO(adapterInfo);
                WD_INFO(vramInfo);
                WD_INFO(vendorInfo);

                break;
            }

            // If the adapter doesn't support DirectX 12 than we release it
            m_adapter->Release();
        }
    }

    void GfxContext::CreateDevice()
    {
        ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));

        // Create device 2 for it's features
        ThrowIfFailed(m_device->QueryInterface(IID_PPV_ARGS(&m_device2)));

#if defined(_DEBUG)
        ThrowIfFailed(m_device->QueryInterface(IID_PPV_ARGS(&m_debugDevice)));
#endif
    }

    void GfxContext::CreateSwapchain()
    {
        if (m_swapchain)
        {
            m_swapchain->ResizeBuffers(
                BACK_BUFFER_COUNT, m_window->GetWidth(), m_window->GetHeight(), DXGI_FORMAT_UNKNOWN, 0); // ThrowIfFailed();
        }
        else
        {
            DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
            swapchainDesc.BufferCount = BACK_BUFFER_COUNT;
            swapchainDesc.Width = m_window->GetWidth();
            swapchainDesc.Height = m_window->GetHeight();
            swapchainDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
            swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapchainDesc.SampleDesc.Count = 1;

            IDXGISwapChain1* new_swapchain{};

            ThrowIfFailed(m_factory->CreateSwapChainForHwnd(GetCommandQueue(QueueType::Direct)->GetQueue().Get(),
                                                            m_window->GetHandle(),
                                                            &swapchainDesc,
                                                            nullptr,
                                                            nullptr,
                                                            &new_swapchain),
                          "Swapchain creation failed");

            m_swapchain = reinterpret_cast<IDXGISwapChain4*>(new_swapchain);
        }

        for (int i = 0; i < BACK_BUFFER_COUNT; i++)
        {
            CreateTextureFromSwapchain(i);
        }

        TextureDesc desc{};
        desc.width = std::max(m_window->GetWidth(), 1);
        desc.height = std::max(m_window->GetHeight(), 1);

        desc.usage = TextureDesc::gpuOnly;
        desc.flag = TextureDesc::depthStencil;

        m_depthTarget = std::make_shared<Texture>(desc);
    }

    void GfxContext::CreateTextureFromSwapchain(UINT index)
    {
        TextureDesc desc{};
        DXGI_SWAP_CHAIN_DESC1 scDesc;
        m_swapchain->GetDesc1(&scDesc);

        desc.width = m_clientWidth;
        desc.height = m_clientHeight;

        desc.usage = TextureDesc::gpuOnly;
        desc.flag = TextureDesc::renderTarget;

        ComPtr<ID3D12Resource> scBuffer = nullptr;
        m_swapchain->GetBuffer(index, IID_PPV_ARGS(&scBuffer));

        m_renderTargets[index] = std::make_shared<Texture>(desc, scBuffer, D3D12_RESOURCE_STATE_PRESENT);

        std::string rtName = "Backbuffer Render target: " + std::to_string(index);
        m_renderTargets[index]->GetResource()->SetName(StringToWString(rtName).c_str());
    }

    void GfxContext::CreatePipelineLibrary()
    {
        if (m_capabilities.CheckLibraryPSOCacheSupport())
        {
            std::vector<uint8_t> cachedData;
            std::ifstream cacheFile("psoCache.bin", std::ios::binary);

            if (cacheFile.good())
            {
                m_libraryData = std::vector<uint8_t>(std::istreambuf_iterator<char>(cacheFile), std::istreambuf_iterator<char>());
            }

            HRESULT hr =
                m_device2->CreatePipelineLibrary(m_libraryData.data(), m_libraryData.size(), IID_PPV_ARGS(&m_pipelineLibrary));

            if (FAILED(hr))
            {
                if (hr == D3D12_ERROR_DRIVER_VERSION_MISMATCH || hr == D3D12_ERROR_ADAPTER_NOT_FOUND ||
                    hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR)
                {
                    WD_WARN("PSO cache invalid (0x{:#x}), rebuilding...", static_cast<uint32_t>(hr));
                    std::filesystem::remove("psoCache.bin"); // delete corrupt file
                    ThrowIfFailed(m_device2->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_pipelineLibrary)));
                    m_usesLegacyCache = true;
                }
                else if (hr == E_INVALIDARG)
                {
                    WD_WARN("PSO cache invalid (0x{:#x}), rebuilding...", static_cast<uint32_t>(hr));
                    std::filesystem::remove("psoCache.bin"); // delete corrupt file
                    ThrowIfFailed(m_device2->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&m_pipelineLibrary)));
                }
                else
                {
                    ThrowIfFailed(hr);
                }
            }
        }
    }

    void GfxContext::CachePipelineLibrary()
    {
        if (m_capabilities.CheckLibraryPSOCacheSupport())
        {
            if (!m_pipelineLibrary) { WD_FATAL("Pipeline library was not created!"); }

            SIZE_T size = m_pipelineLibrary->GetSerializedSize();
            if (size == 0) return;

            std::vector<uint8_t> data(size);
            ThrowIfFailed(m_pipelineLibrary->Serialize(data.data(), size));

            std::ofstream file("psoCache.bin.tmp", std::ios::binary);
            if (!file)
            {
                WD_ERROR("Failed to open psoCache.bin for writing");
                return;
            }

            file.write(reinterpret_cast<const char*>(data.data()), data.size());
            if (!file) WD_ERROR("Failed to write PSO cache to disk");
            file.close();

            std::filesystem::remove("psoCache.bin");
            std::filesystem::rename("psoCache.bin.tmp", "psoCache.bin");

            WD_INFO("PSO is cached.");
        }
    }
} // namespace Wild
