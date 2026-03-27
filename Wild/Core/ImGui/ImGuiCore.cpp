#include "Core/ImGui/ImGuiCore.hpp"
#include "Renderer/Passes/PbrPass.hpp"
#include "Renderer/Resources/Mesh.hpp"

#include <imgui_internal.h>

#include "Tools/Common3d12.hpp"

#include <pix3.h>

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

    void ImguiCore::AddPanel(const std::string& name, std::function<void()> renderFunc)
    {
        m_panels[name] = renderFunc;
        if (m_panelVisible.find(name) == m_panelVisible.end()) m_panelVisible[name] = true;
    }

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

    void ImguiCore::DrawViewport(Renderer* renderer)
    {
        if (m_fullscreen)
        {
            ImVec2 displaySize = ImGui::GetIO().DisplaySize;
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(displaySize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("##FullscreenViewport",
                         nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNavFocus);
            ImGui::Image((ImTextureID)renderer->compositeTexture->GetSrv()->GetGpuHandle().ptr, displaySize);
            ImGui::End();
            ImGui::PopStyleVar();
        }
        else
        {
            for (size_t i = 0; i < 1; i++)
            {
                if (renderer->compositeTexture)
                {
                    ImGui::SetNextWindowContentSize(ImVec2(0.0f, 0.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                    std::string window_label = "Viewport " + std::to_string(i);
                    ImGui::Begin(
                        window_label.c_str(), nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                    // Get the available size of the content region
                    ImVec2 scenePos = ImGui::GetWindowPos();
                    ImVec2 sceneSize = ImGui::GetWindowSize();

                    ImGui::SetCursorPos(ImVec2(0, 0));
                    ImGui::Image((ImTextureID)renderer->compositeTexture->GetSrv()->GetGpuHandle().ptr, sceneSize);

                    ImGui::End();
                    ImGui::PopStyleVar();
                }
            }
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
        initInfo.RTVFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
        initInfo.SrvDescriptorHeap = gfxContext->GetCbvSrvUavAllocator()->GetHeap().Get();
        initInfo.LegacySingleSrvCpuDescriptor =
            gfxContext->GetCbvSrvUavAllocator()->GetHeap()->GetCPUDescriptorHandleForHeapStart();
        initInfo.LegacySingleSrvGpuDescriptor =
            gfxContext->GetCbvSrvUavAllocator()->GetHeap()->GetGPUDescriptorHandleForHeapStart();

        ImGui_ImplDX12_Init(&initInfo);

        ImPlot::CreateContext();

        return true;
    }

    void ImguiCore::DisplayTexture(std::shared_ptr<Texture> texture, glm::vec2 imageSize)
    {
        if (texture) ImGui::Image((ImTextureID)texture->GetSrv()->GetGpuHandle().ptr, ImVec2(imageSize.x, imageSize.y));
    }

    void ImguiCore::DisplayTexture(Texture* texture, glm::vec2 imageSize)
    {
        if (texture) ImGui::Image((ImTextureID)texture->GetSrv()->GetGpuHandle().ptr, ImVec2(imageSize.x, imageSize.y));
    }

    void ImguiCore::DrawMenuBar()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 6.0f));
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("New Scene", "Ctrl+N")) {} // TODO implement scene selecting
                if (ImGui::BeginMenu("Open Scene"))
                {
                    for (const auto& name : engine.GetSceneManager()->GetSceneNames())
                    {
                        if (ImGui::MenuItem(name.c_str())) { engine.GetSceneManager()->LoadScene(name); }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {} // TODO implement scene selecting
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) PostQuitMessage(0);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) {}
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Select All", "Ctrl+A")) {}
                if (ImGui::MenuItem("Delete Entity", "Del", false, m_selectedEntity != entt::null))
                {
                    engine.GetECS()->GetRegistry().destroy(m_selectedEntity);
                    m_selectedEntity = entt::null;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::SeparatorText("Panels");
                for (auto& [name, visible] : m_panelVisible)
                    ImGui::MenuItem(name.c_str(), nullptr, &visible);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Engine"))
            {
                // ImGui::SeparatorText("Rendering");
                // if (ImGui::MenuItem("Wireframe", nullptr, &m_wireframe))
                //{
                //     // TODO add wireframe mode
                // }

                ImGui::SeparatorText("Gizmo Mode");
                if (ImGui::MenuItem("World Space", nullptr, m_gizmoMode == ImGuizmo::WORLD)) m_gizmoMode = ImGuizmo::WORLD;
                if (ImGui::MenuItem("Local Space", nullptr, m_gizmoMode == ImGuizmo::LOCAL)) m_gizmoMode = ImGuizmo::LOCAL;

                ImGui::SeparatorText("Scene");
                if (ImGui::MenuItem("Spawn Point Light"))
                {
                    auto e = engine.GetECS()->CreateEntity();
                    auto& transform = engine.GetECS()->AddComponent<Transform>(e, e);
                    transform.Name = "Point light";
                    engine.GetECS()->AddComponent<PointLight>(e);
                }

                ImGui::EndMenu();
            }
            // ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 6.0f));

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0, 0, 0, 0));

            ImVec2 barPos = ImGui::GetWindowPos();
            ImVec2 barSize = ImGui::GetWindowSize();

            float panelHeight = barSize.y - 8.0f;
            float panelWidth = barSize.x * 0.6f;
            auto panelPos = ImVec2(barPos.x + (barSize.x - panelWidth) * 0.5f, barPos.y + 4.0f);
            auto panelEnd = ImVec2(panelPos.x + panelWidth, panelPos.y + panelHeight);

            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(panelPos, panelEnd, IM_COL32(40, 40, 40, 255), 6.0f);

            ImGui::SameLine((ImGui::GetContentRegionMax().x / 2.f) - 15.f);

            ImGui::PopStyleColor(3);

            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar();
    }

    void ImguiCore::DrawInspectorWindow()
    {
        auto& ecs = engine.GetECS();
        auto& registry = ecs->GetRegistry();

        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("Inspector");

        if (m_selectedEntity == entt::null)
        {
            ImGui::Text("No entity selected");

            ImGui::End();
            return;
        }

        ImGui::Text("Name:");
        ImGui::Text("Entity ID: %u", entt::to_integral(m_selectedEntity));

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ecs->HasComponent<Transform>(m_selectedEntity))
            {
                auto& transform = registry.get<Transform>(m_selectedEntity);

                // Position
                if (SeperateDragFloat3("Position", transform.GetPosition())) { transform.MarkDirty(); };

                glm::vec3 eulerRotation = glm::degrees(glm::eulerAngles(transform.GetRotation()));

                // Rotation
                if (SeperateDragFloat3("Rotation", eulerRotation))
                {
                    transform.SetRotation(glm::quat(glm::radians(eulerRotation)));
                    transform.MarkDirty();
                }

                // Scale
                if (SeperateDragFloat3("Scale", transform.GetScale())) { transform.MarkDirty(); }
            }
        }

        if (ecs->HasComponent<Mesh>(m_selectedEntity))
        {
            if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& mesh = registry.get<Mesh>(m_selectedEntity);

                auto& material = mesh.GetMaterial();

                DisplayTexture(material.m_albedo);
                DisplayTexture(material.m_normal);
                DisplayTexture(material.m_emissive);
                DisplayTexture(material.m_roughnessMetallic);
                DisplayTexture(material.m_occlusion);

                ImGui::Separator();
            }
        }
        static bool Update = false;

        if (ecs->HasComponent<PointLight>(m_selectedEntity))
        {
            if (ImGui::CollapsingHeader("Point light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& ptLight = registry.get<PointLight>(m_selectedEntity);

                ImGui::ColorEdit3("Point Light Color: ", &ptLight.colorIntensity[0]);
                ImGui::DragFloat("Intensity: ", &ptLight.colorIntensity.w);
            }
        }

        if (ecs->HasComponent<DirectionalLight>(m_selectedEntity))
        {
            if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& dLight = registry.get<DirectionalLight>(m_selectedEntity);

                glm::vec3 dir = glm::normalize(glm::vec3(dLight.direction[0], dLight.direction[1], dLight.direction[2]));

                float pitch = glm::degrees(asinf(glm::clamp(dir.y, -1.0f, 1.0f)));
                float yaw = glm::degrees(atan2f(dir.x, dir.z));

                float euler[2] = {pitch, yaw};

                if (ImGui::DragFloat2("Rotation: (P/Y) ", euler, 0.5f, -180.0f, 180.0f, "%.1f deg"))
                {
                    float p = glm::radians(euler[0]);
                    float y = glm::radians(euler[1]);

                    dLight.direction[0] = cosf(p) * sinf(y);
                    dLight.direction[1] = sinf(p);
                    dLight.direction[2] = cosf(p) * cosf(y);
                }

                ImGui::ColorEdit3("Color", &dLight.colorIntensity[0]);
                ImGui::SliderFloat("Intensity", &dLight.colorIntensity.w, 0, 50.0f);
            }
        }

        ImGui::End();
    }

    bool ImguiCore::SeperateDragFloat3(const char* name, glm::vec3& value)
    {
        ImGui::PushID(name);
        ImGui::BeginGroup();
        ImGui::PushItemWidth((ImGui::CalcItemWidth() - 6) / 4.0f);
        bool isChanged = false;

        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.05f, 0.05f, 1.0f));
            isChanged |= ImGui::DragFloat("##x", &value.x, 0.1f);
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::PopStyleColor(2);
        }

        {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.2f, 0.05f, 1.0f));
            isChanged |= ImGui::DragFloat("##y", &value.y, 0.1f);
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
            ImGui::PopStyleColor(2);
        }

        {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.2f, 1.0f));
            isChanged |= ImGui::DragFloat("##z", &value.z, 0.1);
            ImGui::PopStyleColor(2);
        }

        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextUnformatted(name);

        ImGui::PopStyleVar();
        ImGui::PopItemWidth();
        ImGui::EndGroup();
        ImGui::PopID();
        return isChanged;
    }

    void ImguiCore::AddStatsBar()
    {
        ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

        const float barHeight = 20.0f;
        ImGui::BeginViewportSideBar("##StatsBar", ImGui::GetMainViewport(), ImGuiDir_Down, barHeight, window_flags);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wpos = ImGui::GetWindowPos();
        ImVec2 wsz = ImGui::GetWindowSize();

        ImU32 bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
        ImU32 outline = ImGui::GetColorU32(ImGuiCol_Border);
        const float pad = 4.0f;
        const float rounding = 6.0f;
        dl->AddRectFilled(ImVec2(wpos.x + pad, wpos.y + pad),
                          ImVec2(wpos.x + wsz.x - pad, wpos.y + wsz.y - pad),
                          ImGui::GetColorU32(ImGuiCol_FrameBgActive),
                          rounding);
        // thin outline
        dl->AddRect(ImVec2(wpos.x + pad, wpos.y + pad),
                    ImVec2(wpos.x + wsz.x - pad, wpos.y + wsz.y - pad),
                    outline,
                    rounding,
                    ImDrawFlags_RoundCornersAll,
                    1.0f);

        ImGui::BeginMenuBar();

        auto& context = engine.GetGfxContext();
        std::string deviceName = context->GetAdapterName();
        std::string deviceDisplay = "Device: " + deviceName;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
        ImGui::TextUnformatted(deviceDisplay.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::TextUnformatted("Version: 0.2");
        ImGui::PopStyleVar();

        // compute FPS and color
        float fps = ImGui::GetIO().Framerate;
        float ms = ImGui::GetIO().DeltaTime * 1000.0f;

        ImVec4 fps_col;
        if (fps >= 55.0f)
            fps_col = ImVec4(0.4f, 0.95f, 0.4f, 1.0f);
        else if (fps >= 30.0f)
            fps_col = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
        else
            fps_col = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);

        // move to right edge area
        float availX = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 360.0f);

        // show FPS and ms
        ImGui::PushStyleColor(ImGuiCol_Text, fps_col);
        ImGui::Text("fps: %3.1f", fps);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("%2.2f ms", ms);

        ImGui::EndMenuBar();
        ImGui::End();
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

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver())
        {
            m_selectedEntity = entt::null;
        }

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

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) m_fullscreen = !m_fullscreen;

        if (!m_fullscreen)
        {
            ImGui::DockSpaceOverViewport(ImGui::GetID("DockSpace"), nullptr, ImGuiDockNodeFlags_None);
            DrawMenuBar();
            AddStatsBar();
        }
    }

    void ImguiCore::Draw()
    {
        if (!m_fullscreen)
        {
            DrawInspectorWindow();
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

                if (ImGui::RadioButton("Translate", m_gizmoOperation == ImGuizmo::TRANSLATE))
                    m_gizmoOperation = ImGuizmo::TRANSLATE;

                if (ImGui::RadioButton("Rotate", m_gizmoOperation == ImGuizmo::ROTATE)) m_gizmoOperation = ImGuizmo::ROTATE;

                if (ImGui::RadioButton("Scale", m_gizmoOperation == ImGuizmo::SCALE)) m_gizmoOperation = ImGuizmo::SCALE;
            });

            // Draw panels
            for (auto& [name, func] : m_panels)
            {
                if (!m_panelVisible[name]) continue;
                if (ImGui::Begin(name.c_str(), &m_panelVisible[name])) { func(); }
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
        }

#ifdef DEBUG
        std::string passName = "Imgui pass";
        std::wstring wstringPassName(passName.begin(), passName.end());
        PIXBeginEvent(engine.GetGfxContext()->GetCommandList()->GetList().Get(), PIX_COLOR_INDEX(1), wstringPassName.c_str());
#endif

        ImGui::Render();

        auto list = engine.GetGfxContext()->GetCommandList()->GetList();

        ID3D12DescriptorHeap* heaps[] = {engine.GetGfxContext()->GetCbvSrvUavAllocator()->GetHeap().Get()};
        list->SetDescriptorHeaps(_countof(heaps), heaps);

        // Render ImGui draw data
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list.Get());

#ifdef DEBUG
        PIXEndEvent(engine.GetGfxContext()->GetCommandList()->GetList().Get());
#endif
    }
} // namespace Wild
