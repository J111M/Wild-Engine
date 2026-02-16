#include "Tools/Profiler/Profiler.hpp"

#include <imgui.h>
#include <implot/implot.h>

namespace Wild
{
#ifdef DEBUG
    ProfileScope::ProfileScope(const std::string& name) : m_name(name) { engine.GetProfiler()->BeginProfile(m_name); }

    ProfileScope::~ProfileScope() { engine.GetProfiler()->EndProfile(m_name); }

    void Profiler::BeginProfile(const std::string& name)
    {
        m_profiledTime[name].startTime = std::chrono::high_resolution_clock::now();
    }

    void Profiler::EndProfile(const std::string& name)
    {
        auto& map = m_profiledTime[name];
        map.endTime = std::chrono::high_resolution_clock::now();
        auto elapsed = map.endTime - map.startTime;
        map.accumilation += elapsed;
    }

    void Profiler::Display()
    {
        for (auto& i : m_profiledTime)
        {
            auto& e = i.second;
            auto duration = (float)((double)e.accumilation.count() / 1e+6);
            if (e.history.size() > 300) e.history.pop_front();
            e.history.push_back(duration);

            e.average = 0.0f;
            e.min = FLT_MAX;
            e.max = FLT_MIN;

            for (float f : e.history)
            {
                e.average += f;

                // Get min and max profile times for best and worst cases
                if (f < e.min) e.min = f;
                if (f > e.max) e.max = f;
            }

            e.average /= (float)e.history.size();
        }

        if (ImGui::Begin("Profiler"))
        {
            if (ImPlot::BeginPlot("Profiler"))
            {
                ImPlot::SetupAxes("Sample", "Time");
                ImPlot::SetupAxesLimits(0, 300, 0, 3);

                for (auto& i : m_profiledTime)
                {
                    auto& e = i.second;

                    std::vector<float> vals(e.history.begin(), e.history.end());

                    ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, 0.25f);
                    ImPlot::PlotShaded(i.first.c_str(), vals.data(), (int)vals.size());
                    ImPlot::PopStyleVar();
                    ImPlot::PlotLine(i.first.c_str(), vals.data(), (int)vals.size());
                }
                ImPlot::EndPlot();

                for (auto& i : m_profiledTime)
                {
                    auto& e = i.second;

                    if (ImGui::TreeNode(i.first.c_str(), "%s: %.3f ms", i.first.c_str(), e.average))
                    {
                        ImGui::Text("Average: %.3f ms", e.average);
                        ImGui::Text("Min:     %.3f ms", e.min);
                        ImGui::Text("Max:     %.3f ms", e.max);
                        ImGui::Text("Latest:  %.3f ms", e.history.empty() ? 0.0f : e.history.back());
                        ImGui::Text("Samples: %d", (int)e.history.size());
                        ImGui::TreePop();
                    }

                    i.second.accumilation = {};
                }
            }
        }
        ImGui::End();
    }
#else
    ProfileScope::ProfileScope(const std::string& name) {}
    ProfileScope::~ProfileScope() {}
    void Profiler::BeginProfile(const std::string& name) {}
    void Profiler::EndProfile(const std::string& name) {}
    void Profiler::Display() {}
#endif
} // namespace Wild
