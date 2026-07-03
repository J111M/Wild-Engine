#include "Editor/Panels/ConsolePanel.hpp"

#include <imgui.h>

namespace Wild
{
    namespace
    {
        ImVec4 LevelColor(spdlog::level::level_enum level)
        {
            switch (level)
            {
            case spdlog::level::trace:
                return ImVec4(0.55f, 0.55f, 0.58f, 1.0f);
            case spdlog::level::debug:
                return ImVec4(0.45f, 0.65f, 0.80f, 1.0f);
            case spdlog::level::info:
                return ImVec4(0.80f, 0.80f, 0.82f, 1.0f);
            case spdlog::level::warn:
                return ImVec4(0.90f, 0.75f, 0.30f, 1.0f);
            case spdlog::level::err:
                return ImVec4(0.90f, 0.40f, 0.40f, 1.0f);
            case spdlog::level::critical:
                return ImVec4(1.00f, 0.25f, 0.25f, 1.0f);
            default:
                return ImVec4(0.80f, 0.80f, 0.82f, 1.0f);
            }
        }

        const char* LevelNames[] = {"Trace", "Debug", "Info", "Warn", "Error", "Critical"};
    } // namespace

    void ConsolePanel::OnRender()
    {
        // Pull whatever the sink collected since last frame (any thread may
        // have logged from here on everything is main-thread only)
        EditorLogSink::Instance()->Drain(m_entries);
        if (m_entries.size() > MaxEntries)
            m_entries.erase(m_entries.begin(), m_entries.begin() + (m_entries.size() - MaxEntries));

        // Toolbar
        if (ImGui::Button("Clear")) m_entries.clear();
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("##MinLevel", &m_minLevel, LevelNames, IM_ARRAYSIZE(LevelNames));
        ImGui::SameLine();
        m_textFilter.Draw("##Filter", -1.0f);

        ImGui::Separator();

        if (ImGui::BeginChild("##ConsoleScroll", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
        {
            for (const auto& entry : m_entries)
            {
                if ((int)entry.level < m_minLevel) continue;
                if (!m_textFilter.PassFilter(entry.text.c_str())) continue;

                ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(entry.level));
                ImGui::TextUnformatted(entry.text.c_str());
                ImGui::PopStyleColor();
            }

            if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
} // namespace Wild
