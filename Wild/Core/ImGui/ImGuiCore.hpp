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

		template<typename T>
		void Watch(const std::string& name, T* value);

		void AddPanel(const std::string& name, std::function<void()> renderFunc);

	private:
		bool Setup(std::shared_ptr<Window> window);

		std::unordered_map<std::string, std::function<void()>> m_panels;
		std::unordered_map<std::string, std::function<void()>> m_watches;
	};

	template<typename T>
	inline void ImguiCore::Watch(const std::string& name, T* value)
	{
		m_watches[name] = [value]() {
			if constexpr (std::is_same_v<T, float>) {
				ImGui::Text("%s: %.2f", name.c_str(), *value);
			}
			else if constexpr (std::is_same_v<T, int>) {
				ImGui::Text("%s: %d", name.c_str(), *value);
			}
			else if constexpr (std::is_same_v<T, uint32_t>) {
				ImGui::Text("%s: %d", name.c_str(), *value);
			}
		};
	}
}