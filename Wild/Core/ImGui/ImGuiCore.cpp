#include "Core/ImGui/ImGuiCore.hpp"

#include "Core/Engine.hpp"
#include "Core/Transform.hpp"
#include "Editor/EditorTheme.hpp"
#include "Editor/EditorWidgets.hpp"
#include "Editor/Panels/AssetBrowserPanel.hpp"
#include "Editor/Panels/ConsolePanel.hpp"
#include "Editor/Panels/DebugPanel.hpp"
#include "Editor/Panels/HierarchyPanel.hpp"
#include "Editor/Panels/InspectorPanel.hpp"
#include "Editor/Panels/ProfilerPanel.hpp"
#include "Editor/Panels/SystemPanels.hpp"
#include "Editor/Panels/ViewportPanel.hpp"
#include "Renderer/Passes/PbrPass.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/Window.hpp"
#include "Tools/D3D12Common.hpp"

#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_glfw.h"
#include <imgui_internal.h>

#include <pix3.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

namespace Wild
{
    namespace
    {
        void EditorGlfwDropCallback(GLFWwindow*, int count, const char** paths)
        {
            if (auto imgui = engine.GetImGui()) imgui->OnFilesDropped(paths, count);
        }
    } // namespace

    ImguiCore::ImguiCore(std::shared_ptr<Window> window) : m_window(std::move(window))
    {
        Setup(m_window);
        RegisterPanels();
    }

    ImguiCore::~ImguiCore()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    bool ImguiCore::Setup(std::shared_ptr<Window> window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // enable here for multi-window support,
        //                                                        Draw() already handles the platform windows

        io.IniFilename = "ImGuiSettings.ini";

        m_rebuildLayout = !std::filesystem::exists(io.IniFilename);

        LoadEditorFont();
        ApplyEditorTheme();

        ImGui_ImplGlfw_InitForOther(window->GetWindow(), true);

        // Route OS file drops into the Assets panel
        glfwSetDropCallback(window->GetWindow(), EditorGlfwDropCallback);

        auto gfxContext = engine.GetGfxContext();

        ImGui_ImplDX12_InitInfo initInfo = {};
        initInfo.Device = gfxContext->GetDevice().Get();
        initInfo.CommandQueue = gfxContext->GetCommandQueue(QueueType::Direct)->GetQueue().Get();
        initInfo.NumFramesInFlight = BACK_BUFFER_COUNT;
        initInfo.RTVFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
        initInfo.SrvDescriptorHeap = gfxContext->GetCbvSrvUavAllocator()->GetHeap().Get();

        // Loading icons
        initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
                                           D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
            auto allocator = engine.GetGfxContext()->GetCbvSrvUavAllocator();
            const uint32_t handle = allocator->AllocateHandle();
            *outCpuHandle = allocator->CpuHandle(handle);
            *outGpuHandle = allocator->GpuHandle(handle);
        };
        initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                                          D3D12_GPU_DESCRIPTOR_HANDLE) {
            auto allocator = engine.GetGfxContext()->GetCbvSrvUavAllocator();
            const uint32_t handle = static_cast<uint32_t>(
                (cpuHandle.ptr - allocator->GetHeap()->GetCPUDescriptorHandleForHeapStart().ptr) /
                allocator->GetIncrementSize());
            allocator->FreeHandle(handle);
        };

        ImGui_ImplDX12_Init(&initInfo);

        ImPlot::CreateContext();

        return true;
    }

    void ImguiCore::RegisterPanels()
    {
        m_panels.Register(std::make_unique<HierarchyPanel>(m_state));
        m_panels.Register(std::make_unique<InspectorPanel>(m_state));

        m_viewportPanel = static_cast<ViewportPanel*>(m_panels.Register(std::make_unique<ViewportPanel>(m_state)));
        m_assetBrowserPanel = static_cast<AssetBrowserPanel*>(m_panels.Register(std::make_unique<AssetBrowserPanel>()));
        m_panels.Register(std::make_unique<ConsolePanel>());
        m_profilerPanel = static_cast<ProfilerPanel*>(m_panels.Register(std::make_unique<ProfilerPanel>()));

        m_panels.Register(std::make_unique<OceanSystemPanel>());
        m_panels.Register(std::make_unique<VolumetricsPanel>());

        m_panels.Register(std::make_unique<DebugPanel>(m_state, m_watches));
    }

    void ImguiCore::Prepare()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) m_fullscreen = !m_fullscreen;

        if (m_fullscreen) return;

        ImGuiID dockspaceId = ImGui::GetID("DockSpace");
        ImGui::DockSpaceOverViewport(dockspaceId, nullptr, ImGuiDockNodeFlags_None);

        if (m_rebuildLayout)
        {
            BuildDefaultLayout(dockspaceId);
            m_rebuildLayout = false;
        }

        DrawMenuBar();
        DrawStatsBar();
    }

    void ImguiCore::BuildDefaultLayout(ImGuiID dockspaceId)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->WorkSize);

        ImGuiID dockMain = dockspaceId;
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.24f, nullptr, &dockMain);
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);

        ImGui::DockBuilderDockWindow("Hierarchy", dockLeft);
        ImGui::DockBuilderDockWindow("Inspector", dockRight);
        ImGui::DockBuilderDockWindow("Viewport", dockMain);
        ImGui::DockBuilderDockWindow("Assets", dockBottom);
        ImGui::DockBuilderDockWindow("Console", dockBottom);
        ImGui::DockBuilderDockWindow("Profiler", dockBottom);
        ImGui::DockBuilderDockWindow("Debug", dockRight);
        ImGui::DockBuilderDockWindow("Ocean Rendering", dockRight);
        ImGui::DockBuilderDockWindow("Volumetrics", dockRight);

        // Panels the render passes register at runtime
        ImGui::DockBuilderDockWindow("Ocean Settings", dockRight);
        ImGui::DockBuilderDockWindow("Shadowmap settings", dockRight);
        ImGui::DockBuilderDockWindow("Shadowmap Textures", dockBottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void ImguiCore::DrawMenuBar()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 6.0f));
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene", "Ctrl+N")) {} // TODO implement scene creation
                if (ImGui::BeginMenu("Open Scene"))
                {
                    for (const auto& name : engine.GetSceneManager()->GetSceneNames())
                    {
                        if (ImGui::MenuItem(name.c_str())) engine.GetSceneManager()->LoadScene(name);
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {} // TODO implement scene saving
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) glfwSetWindowShouldClose(m_window->GetWindow(), GLFW_TRUE);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {}
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Entity", "Del", false, m_state.selectedEntity != entt::null))
                {
                    engine.GetECS()->GetRegistry().destroy(m_state.selectedEntity);
                    m_state.selectedEntity = entt::null;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window"))
            {
                m_panels.RenderWindowMenu();
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Layout")) m_rebuildLayout = true;
                ImGui::MenuItem("Fullscreen Viewport", "Ctrl+F", &m_fullscreen);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Engine"))
            {
                ImGui::SeparatorText("Gizmo Mode");
                if (ImGui::MenuItem("World Space", nullptr, m_state.gizmoMode == ImGuizmo::WORLD))
                    m_state.gizmoMode = ImGuizmo::WORLD;
                if (ImGui::MenuItem("Local Space", nullptr, m_state.gizmoMode == ImGuizmo::LOCAL))
                    m_state.gizmoMode = ImGuizmo::LOCAL;

                ImGui::SeparatorText("Scene");
                if (ImGui::MenuItem("Spawn Point Light"))
                {
                    auto entity = engine.GetECS()->CreateEntity();
                    auto& transform = engine.GetECS()->AddComponent<Transform>(entity, entity);
                    transform.Name = "Point light";
                    engine.GetECS()->AddComponent<PointLight>(entity);
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help"))
            {
                ImGui::TextDisabled("Wild Engine 0.2");
                ImGui::TextDisabled("Ctrl+F toggles the fullscreen viewport");
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar();
    }

    void ImguiCore::DrawStatsBar()
    {
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

        const float barHeight = 24.0f;
        if (ImGui::BeginViewportSideBar("##StatsBar", ImGui::GetMainViewport(), ImGuiDir_Down, barHeight, windowFlags))
        {
            if (ImGui::BeginMenuBar())
            {
                std::string deviceDisplay = "Device: " + engine.GetGfxContext()->GetAdapterName();

                ImGui::TextUnformatted(deviceDisplay.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("|");
                ImGui::SameLine();
                ImGui::TextUnformatted("Version: 0.2");

                float fps = ImGui::GetIO().Framerate;
                float ms = ImGui::GetIO().DeltaTime * 1000.0f;

                ImVec4 fpsColor;
                if (fps >= 55.0f)
                    fpsColor = ImVec4(0.4f, 0.95f, 0.4f, 1.0f);
                else if (fps >= 30.0f)
                    fpsColor = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
                else
                    fpsColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

                char stats[64];
                snprintf(stats, sizeof(stats), "%.1f fps | %.2f ms", fps, ms);

                float statsWidth = ImGui::CalcTextSize(stats).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
                ImGui::SetCursorPosX(ImGui::GetWindowSize().x - statsWidth);

                ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
                ImGui::TextUnformatted(stats);
                ImGui::PopStyleColor();

                ImGui::EndMenuBar();
            }
        }
        ImGui::End();
    }

    void ImguiCore::DrawGizmo(const glm::mat4& view, const glm::mat4& projection)
    {
        if (m_state.selectedEntity == entt::null) return;
        if (!engine.GetECS()->GetRegistry().valid(m_state.selectedEntity)) return;

        auto& transform = engine.GetECS()->GetComponent<Transform>(m_state.selectedEntity);
        auto model = transform.GetWorldMatrix();

        if (!m_fullscreen && m_state.viewportSize.x > 0.0f)
            ImGuizmo::SetRect(m_state.viewportPos.x, m_state.viewportPos.y, m_state.viewportSize.x, m_state.viewportSize.y);
        else
            ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection), m_state.gizmoOperation, m_state.gizmoMode, glm::value_ptr(model));

        if (ImGuizmo::IsUsing())
        {
            if (transform.HasParent())
            {
                auto& parentTransform = engine.GetECS()->GetRegistry().get<Transform>(transform.GetParent());
                model = glm::inverse(parentTransform.GetWorldMatrix()) * model;
            }
            transform.SetFromMatrix(model);
        }
    }

    void ImguiCore::DrawViewport(Renderer* renderer)
    {
        ImTextureID texture{};
        if (renderer && renderer->compositeTexture)
            texture = (ImTextureID)renderer->compositeTexture->GetSrv()->GetGpuHandle().ptr;

        if (!m_fullscreen)
        {
            m_viewportPanel->SetTexture(texture);
            return;
        }

        if (!texture) return;

        ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(displaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##FullscreenViewport",
                     nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoNavFocus);
        ImGui::Image(texture, displaySize);
        ImGui::End();
        ImGui::PopStyleVar();
    }

    void ImguiCore::Draw()
    {
        if (!m_fullscreen) m_panels.RenderPanels();

#ifdef DEBUG
        PIXBeginEvent(engine.GetGfxContext()->GetCommandList()->GetList().Get(), PIX_COLOR_INDEX(1), L"Imgui pass");
#endif

        ImGui::Render();

        auto list = engine.GetGfxContext()->GetCommandList()->GetList();

        ID3D12DescriptorHeap* heaps[] = {engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get()};
        list->SetDescriptorHeaps(_countof(heaps), heaps);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list.Get());

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

#ifdef DEBUG
        PIXEndEvent(engine.GetGfxContext()->GetCommandList()->GetList().Get());
#endif
    }

    void ImguiCore::AddPanel(const std::string& name, std::function<void()> renderFunc)
    { m_panels.AddCallbackPanel(name, "Systems", std::move(renderFunc)); }

    void ImguiCore::SetProfilerDrawCallback(std::function<void()> callback)
    { m_profilerPanel->SetDrawCallback(std::move(callback)); }

    void ImguiCore::OnFilesDropped(const char** paths, int count) { m_assetBrowserPanel->AddDroppedFiles(paths, count); }

    void ImguiCore::DisplayTexture(std::shared_ptr<Texture> texture, glm::vec2 imageSize)
    { EditorWidgets::TextureImage(texture, imageSize); }

    void ImguiCore::DisplayTexture(Texture* texture, glm::vec2 imageSize) { EditorWidgets::TextureImage(texture, imageSize); }
} // namespace Wild
