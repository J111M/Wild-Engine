#include "Core/Engine.hpp"

#include "Tools/Log.hpp"

namespace Wild
{
    void Engine::Initialize()
    {
        WD_INFO("Wild engine initialized.");

        m_window = std::make_shared<Window>("Wild engine", 1200, 800);
        m_gfxContext = std::make_shared<GfxContext>(m_window);
        m_gfxContext->Initialize();

        m_ecs = std::make_shared<EntityComponentSystem>();
        m_imguiCore = std::make_shared<ImguiCore>(m_window);
        m_shaderTracker = std::make_shared<ShaderTracker>();
        m_profiler = std::make_shared<Profiler>();

        m_resourceSystems.m_meshResourceSystem = std::make_shared<ResourceSystem<Mesh>>();
        m_resourceSystems.m_textureResourceSystem = std::make_shared<ResourceSystem<Texture>>();

        m_sceneManager = std::make_shared<SceneManager>();

        m_renderer = std::make_shared<Renderer>();
    }

    void Engine::Run()
    {
        auto previousTime = std::chrono::high_resolution_clock::now();
        auto endTime = previousTime;

        {
            auto cameraEntity = engine.GetECS()->CreateEntity();
            auto& transform = engine.GetECS()->AddComponent<Transform>(cameraEntity, glm::vec3(0, 0, 0), cameraEntity);
            transform.Name = "Main camera";

            auto& camera = engine.GetECS()->AddComponent<Camera>(cameraEntity, glm::vec3(0, 0, 5), 0u);
            camera.SetMovementActivity(true);
        }

#ifdef DEBUG
        // Second camera for debugging
        /* {
             auto cameraEntity = engine.GetECS()->CreateEntity();
             auto& transform = engine.GetECS()->AddComponent<Transform>(cameraEntity, glm::vec3(0, 0, 0), cameraEntity);
             transform.Name = "Debug camera";
             auto& camera = engine.GetECS()->AddComponent<Camera>(cameraEntity, glm::vec3(0, 0, 5), 1u);
             camera.SetMovementActivity(false);
         }*/
#endif // DEBUG

        float frameTime{};
        int frameCount = 0;

        while (!m_window->ShouldClose())
        {
            frameCount++;

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - previousTime).count();
            previousTime = currentTime;

            auto& cameras = m_ecs->View<Camera>();

            Camera* mainCamera{};

            for (auto& cameraEntity : cameras)
            {
                if (m_ecs->HasComponent<Camera>(cameraEntity))
                {
                    auto& camera = m_ecs->GetComponent<Camera>(cameraEntity);
                    camera.Input(*m_window.get(), m_window->GetWidth(), m_window->GetHeight(), deltaTime);
                    camera.UpdateMatrix(glm::radians(70.0f), m_window->AspectRatio(), 0.1f, 500.0f);

                    if (camera.GetCameraIndex() == 0u) { mainCamera = &camera; }
                }
            }

            // Check if the window is resized before calling a new frame
            if (m_gfxContext->ResizeWindow()) { m_renderer->FlushResources(); }

            m_gfxContext->BeginFrame();

#ifdef DEBUG
            m_imguiCore->Prepare();

            // Debug variables
            m_imguiCore->Watch("Frame time: ", &frameTime);
            m_imguiCore->Watch("delta time: ", &deltaTime);
            m_imguiCore->Watch("FPS: ", &m_fps);
#endif

            // Update and render all the passes
            m_renderer->Update(deltaTime);
            m_renderer->Render(*m_gfxContext->GetCommandList().get(), deltaTime);

#ifdef DEBUG
            // Displays profiled data
            m_profiler->Display();

            if (mainCamera) { m_imguiCore->DrawGizmo(mainCamera->GetView(), mainCamera->GetProjection()); }

            m_imguiCore->DrawViewport(m_renderer.get());
            m_imguiCore->Draw();
#endif
            m_gfxContext->EndFrame();

            glfwPollEvents();

            m_renderer->CacheIBLTextures();

            endTime = std::chrono::high_resolution_clock::now();
            frameTime = std::chrono::duration<float, std::milli>(endTime - currentTime).count();
            m_timer += deltaTime * 1;

            SetWindowData(frameCount, frameTime);
        }
        m_gfxContext->CachePipelineLibrary();
    }

    void Engine::Shutdown()
    {
        m_gfxContext->Flush();

        // Explicitly deleting the smart pointers
        m_renderer.reset();
        m_sceneManager.reset();
        m_imguiCore.reset();
        m_ecs.reset();
        m_gfxContext->Shutdown();
        m_gfxContext.reset();
        m_window.reset();
    }

    void Engine::SetWindowData(int& frameCount, float frameTime)
    {
        if (m_timer >= 1.0f)
        {
            m_fps = static_cast<int>(frameCount / m_timer);

            std::string newWindowName =
                "Wild enigne | FPS: " + std::to_string(m_fps) + " | Frame time: " + std::to_string(frameTime) + " ms";
            glfwSetWindowTitle(m_window->GetWindow(), newWindowName.c_str());

            m_timer = 0;
            frameCount = 0;
        }
    }

    void Engine::SetSystemPath(const std::filesystem::path& path) { m_systemPath = path; }

    Engine engine = {};
} // namespace Wild
