#pragma once

#include "Editor/EditorPanel.hpp"

#include <functional>

namespace Wild
{
    class ProfilerPanel final : public EditorPanel
    {
      public:
        ProfilerPanel() { defaultOpen = true; }

        const char* Name() const override { return "Profiler"; }
        const char* Category() const override { return "Core"; }
        void OnRender() override;

        void SetDrawCallback(std::function<void()> callback) { m_drawCallback = std::move(callback); }

      private:
        std::function<void()> m_drawCallback;
        float m_frameHistory[120] = {};
        int m_frameHistoryOffset = 0;
    };
} // namespace Wild
