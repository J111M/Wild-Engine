#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <implot/implot.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_dx12.h"
#include <ImGuizmo.h>

namespace Wild {
	class ImguiCore
	{
	public:
		ImguiCore(std::shared_ptr<Window> window);
		~ImguiCore();

		void Prepare();
		void Draw();

	private:
		bool Setup(std::shared_ptr<Window> window);
	};
}