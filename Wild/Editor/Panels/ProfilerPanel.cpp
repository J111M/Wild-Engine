#include "Editor/Panels/ProfilerPanel.hpp"

#include <imgui.h>

namespace Wild
{
    void ProfilerPanel::OnRender()
    {
        if (m_drawCallback)
        {
            m_drawCallback();
            return;
        }

        float frameMs = ImGui::GetIO().DeltaTime * 1000.0f;
        m_frameHistory[m_frameHistoryOffset] = frameMs;
        m_frameHistoryOffset = (m_frameHistoryOffset + 1) % IM_ARRAYSIZE(m_frameHistory);

        ImGui::Text("Frame time: %.2f ms (%.1f fps)", frameMs, ImGui::GetIO().Framerate);
        ImGui::PlotLines("##FrameTimes", m_frameHistory, IM_ARRAYSIZE(m_frameHistory), m_frameHistoryOffset, nullptr, 0.0f,
                         33.3f, ImVec2(-1.0f, 80.0f));
        ImGui::TextDisabled("No profiler connected, showing frame times only.");
    }
} // namespace Wild
