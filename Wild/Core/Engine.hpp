#pragma once

#include "Renderer/Device.hpp"
#include "Renderer/Renderer.hpp"
#include "Renderer/ShaderPipeline.hpp"
#include "Renderer/Window.hpp"
#include "Renderer/AccelerationStructure.hpp"

#include "Core/Camera.hpp"
#include "Core/ECS.hpp"
#include "Core/ImGui/ImGuiCore.hpp"
#include "Core/Transform.hpp"

#include "Tools/Profiler/Profiler.hpp"

#include "Renderer/Resources/Mesh.hpp"

#include "Systems/ResourceSystem.hpp"
#include "Systems/SceneManager.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include <chrono>
#include <filesystem>

const uint32_t WIDTH = 1600;
const uint32_t HEIGHT = 1000;

namespace Wild
{
    struct ResourceSystems
    {
        std::shared_ptr<ResourceSystem<Texture>> m_textureResourceSystem;
        std::shared_ptr<ResourceSystem<Mesh>> m_meshResourceSystem;
    };

    class Engine
    {
      public:
        void Initialize();
        void Run();
        void Shutdown();

        void SetSystemPath(const std::filesystem::path& path);
        const std::filesystem::path GetSystemPath() & { return m_systemPath; }

        std::shared_ptr<GfxContext> GetGfxContext() { return m_gfxContext; }
        std::shared_ptr<EntityComponentSystem> GetECS() { return m_ecs; }
        std::shared_ptr<ImguiCore> GetImGui() { return m_imguiCore; }
        std::shared_ptr<ShaderTracker> GetShaderTracker() { return m_shaderTracker; }
        std::shared_ptr<Profiler> GetProfiler() { return m_profiler; }
        std::shared_ptr<SceneManager> GetSceneManager() { return m_sceneManager; }
        std::shared_ptr<AccelerationStructureManager> GetAccelerationStructureManager() { return m_accelerationStructureManager; }

        ResourceSystems& GetResourceSystems() { return m_resourceSystems; }

      private:
        void SetWindowData(int& frameCount, float frameTime);

        bool m_shouldClose = false;

        std::shared_ptr<Window> m_window;
        std::shared_ptr<GfxContext> m_gfxContext;
        std::shared_ptr<Renderer> m_renderer;
        std::shared_ptr<EntityComponentSystem> m_ecs;
        std::shared_ptr<ImguiCore> m_imguiCore;
        std::shared_ptr<ShaderTracker> m_shaderTracker;
        std::shared_ptr<Profiler> m_profiler;
        std::shared_ptr<SceneManager> m_sceneManager;
        std::shared_ptr<AccelerationStructureManager> m_accelerationStructureManager;

        ResourceSystems m_resourceSystems;

        std::filesystem::path m_systemPath{};

        float m_timer{};
        int m_fps{};
    };

    extern Engine engine;
} // namespace Wild
