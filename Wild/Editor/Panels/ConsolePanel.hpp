#pragma once

#include "Editor/EditorLogSink.hpp"
#include "Editor/EditorPanel.hpp"

#include <vector>

namespace Wild
{
    class ConsolePanel final : public EditorPanel
    {
      public:
        const char* Name() const override { return "Console"; }
        const char* Category() const override { return "Core"; }
        void OnRender() override;

      private:
        static constexpr size_t MaxEntries = 8192;

        std::vector<EditorLogSink::Entry> m_entries;
        ImGuiTextFilter m_textFilter;
        int m_minLevel = 0; // spdlog::level::trace
        bool m_autoScroll = true;
    };
} // namespace Wild
