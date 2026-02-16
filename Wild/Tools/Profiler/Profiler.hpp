#pragma once

#include <chrono>
#include <deque>
#include <iostream>
#include <string>
#include <unordered_map>

namespace Wild
{
    struct ProfiledData
    {
        std::chrono::time_point<std::chrono::steady_clock> startTime{};
        std::chrono::time_point<std::chrono::steady_clock> endTime{};
        std::chrono::nanoseconds accumilation{};

        float min = FLT_MAX;
        float max = FLT_MIN;

        float average = 0.0f;
        std::deque<float> history;
    };

    class ProfileScope
    {
      public:
        ProfileScope(const std::string& name);
        ~ProfileScope();

      private:
        std::string m_name;
        std::chrono::time_point<std::chrono::steady_clock> m_startTime;
    };

    class Profiler
    {
      public:
        void BeginProfile(const std::string& name);
        void EndProfile(const std::string& name);

        void Display();

      private:
        std::unordered_map<std::string, ProfiledData> m_profiledTime;
    };

#define WD_PROFILESCOPE(name) ProfileScope profiler##__LINE__(name)

} // namespace Wild
