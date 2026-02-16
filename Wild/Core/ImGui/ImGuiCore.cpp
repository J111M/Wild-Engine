#include "Core/ImGui/ImGuiCore.hpp"

#include "Tools/Common3d12.hpp"

namespace Wild
{
    ImguiCore::ImguiCore(std::shared_ptr<Window> window) { Setup(window); }

    ImguiCore::~ImguiCore()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    void ImguiCore::AddPanel(const std::string& name, std::function<void()> renderFunc) { m_panels[name] = renderFunc; }

    void ImguiCore::DrawGizmo(const glm::mat4& view, const glm::mat4& projection)
    {
        if (m_selectedEntity == entt::null) return;

        auto& transform = engine.GetECS()->GetComponent<Transform>(m_selectedEntity);

        auto model = transform.GetWorldMatrix();

        ImGuizmo::SetRect(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection), m_gizmoOperation, m_gizmoMode, glm::value_ptr(model));

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

    bool ImguiCore::Setup(std::shared_ptr<Window> window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
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
        initInfo.LegacySingleSrvCpuDescriptor =
            gfxContext->GetCbvSrvUavAllocator()->GetHeap()->GetCPUDescriptorHandleForHeapStart();
        initInfo.LegacySingleSrvGpuDescriptor =
            gfxContext->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart();

        ImGui_ImplDX12_Init(&initInfo);

        ImPlot::CreateContext();

        return true;
    }

    void ImguiCore::DrawEntityNode(entt::registry& registry, entt::entity entity)
    {
        if (!registry.valid(entity)) return;

        auto& transform = registry.get<Transform>(entity);

        std::string label;

        // Display the entity ID as its name
        if (transform.Name.size() > 0)
            label = transform.Name;
        else
            label = "Entity " + std::to_string(entt::to_integral(entity));

        // Set ImGui TreeNode flags
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (transform.GetChildren().empty()) { flags |= ImGuiTreeNodeFlags_Leaf; }

        if (m_selectedEntity == entity) { flags |= ImGuiTreeNodeFlags_Selected; }

        // Render Tree Node
        bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, label.c_str());

        if (ImGui::IsItemClicked()) { m_selectedEntity = entity; }

        if (nodeOpen)
        {
            // Draw children recursively
            for (auto child : transform.GetChildren())
            {
                DrawEntityNode(registry, child);
            }
            ImGui::TreePop();
        }
    }

    void ImguiCore::ConvertToImGuizmoMatrix(const glm::mat4& glmMatrix, float* guizmoMatrix)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                guizmoMatrix[i + j * 4] = glmMatrix[j][i];
            }
        }
    }

    void ImguiCore::Prepare()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        AddPanel("Hierachy window", [this]() {
            int rowCount = 0;
            float rowHeight = ImGui::GetTextLineHeightWithSpacing();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
            float windHeigth = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;

            ImVec2 windowStartPos = ImGui::GetCursorScreenPos();

            int numRows = static_cast<int>(windHeigth / rowHeight);

            for (size_t i = 0; i < numRows; i++)
            {
                ImVec2 rowPos = ImVec2(windowStartPos.x, windowStartPos.y + i * rowHeight);

                ImU32 bgColor = (rowCount % 2 == 0) ? IM_COL32(2, 2, 2, 255) : // darker gray for even rows
                    IM_COL32(7, 7, 7, 255);                                    // lighter gray for odd rows

                // Draw filled rectangle covering the entire row width.
                drawList->AddRectFilled(rowPos, ImVec2(rowPos.x + windowWidth, rowPos.y + rowHeight), bgColor);

                // Increase the row counter.
                rowCount++;
            }
            auto& registry = engine.GetECS()->GetRegistry();
            registry.view<Transform>().each([&](auto entity, Transform& transform) {
                if (!transform.HasParent()) { DrawEntityNode(registry, entity); }
            });

            if (ImGui::RadioButton("Translate", m_gizmoOperation == ImGuizmo::TRANSLATE)) m_gizmoOperation = ImGuizmo::TRANSLATE;

            if (ImGui::RadioButton("Rotate", m_gizmoOperation == ImGuizmo::ROTATE)) m_gizmoOperation = ImGuizmo::ROTATE;

            if (ImGui::RadioButton("Scale", m_gizmoOperation == ImGuizmo::SCALE)) m_gizmoOperation = ImGuizmo::SCALE;
        });
    }

    void ImguiCore::Draw()
    {
        for (auto& [name, func] : m_panels)
        {
            if (ImGui::Begin(name.c_str())) { func(); }
            ImGui::End();
        }

        // Main debug window
        if (ImGui::Begin("Debug"))
        {
            for (auto& [name, func] : m_watches)
            {
                func();
            }
        }
        ImGui::End();

        ImGui::Render();

        auto list = engine.GetGfxContext()->GetCommandList()->GetList();

        ID3D12DescriptorHeap* heaps[] = {engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get()};
        list->SetDescriptorHeaps(_countof(heaps), heaps);

        // Render ImGui draw data
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list.Get());

        engine.GetGfxContext()->GetCommandList()->EndRender();
    }
} // namespace Wild
