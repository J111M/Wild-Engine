#include "Core/ImGuiCore.hpp"

#include "Tools/Common3d12.hpp"

namespace Wild {
	ImguiCore::ImguiCore(std::shared_ptr<Window> window)
	{
		Setup(window);
	}

	ImguiCore::~ImguiCore()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	bool ImguiCore::Setup(std::shared_ptr<Window> window)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		io.Fonts->AddFontDefault();

		io.IniFilename = "ImGuiSettings.ini";

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForOther(window->GetWindow(), true);

		auto gfxContext = engine.GetGfxContext();

		ImGui_ImplDX12_InitInfo initInfo = {};
		initInfo.Device = gfxContext->GetDevice().Get();
		initInfo.CommandQueue = gfxContext->GetCommandQueue(QueueType::Direct)->GetQueue().Get();
		initInfo.NumFramesInFlight = BACK_BUFFER_COUNT;
		initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		initInfo.SrvDescriptorHeap = gfxContext->GetCbvSrvUavAllocator()->GetHeap().Get();
		initInfo.LegacySingleSrvCpuDescriptor = gfxContext->GetCbvSrvUavAllocator()->GetHeap()->GetCPUDescriptorHandleForHeapStart();
		initInfo.LegacySingleSrvGpuDescriptor = gfxContext->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart();

		ImGui_ImplDX12_Init(&initInfo);

		return true;
	}

	void ImguiCore::Prepare() {
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		static bool show_demo_window = true;
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);
	}

	void ImguiCore::Draw() {
		ImGui::Render();

		auto list = engine.GetGfxContext()->GetCommandList()->GetList();

		ID3D12DescriptorHeap* heaps[] = { engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get() };
		list->SetDescriptorHeaps(_countof(heaps), heaps);

		// Render ImGui draw data
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list.Get());

		engine.GetGfxContext()->GetCommandList()->EndRender();
	}
}