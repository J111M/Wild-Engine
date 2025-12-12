#pragma once

#include "Renderer/Window.hpp"
#include "Renderer/Device.hpp"
#include "Renderer/Renderer.hpp"

#include "Core/ECS.hpp"
#include "Core/Transform.hpp"
#include "Core/Camera.hpp"

#include <memory>
#include <stdexcept>
#include <vector>
#include <iostream>

#include <chrono>

const uint32_t WIDTH = 1600;
const uint32_t HEIGHT = 1000;

namespace Wild {
	class Engine
	{
	public:
		void Initialize();
		void Run();
		void Shutdown();

		std::shared_ptr<GfxContext> GetGfxContext() { return m_gfxContext; }
		std::shared_ptr<EntityComponentSystem> GetECS() { return m_ecs; }
	private:
		bool should_close = false;

		std::shared_ptr<Window> m_window;
		std::shared_ptr<GfxContext> m_gfxContext;
		std::shared_ptr<Renderer> m_renderer;
		std::shared_ptr<EntityComponentSystem> m_ecs;
	};

	extern Engine engine;
}