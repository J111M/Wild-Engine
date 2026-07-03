#pragma once

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Wild
{
    class EditorLogSink final : public spdlog::sinks::base_sink<std::mutex>
    {
      public:
        struct Entry
        {
            spdlog::level::level_enum level = spdlog::level::info;
            std::string text;
        };

        static constexpr size_t MaxPending = 4096;

        static std::shared_ptr<EditorLogSink>& Instance()
        {
            static std::shared_ptr<EditorLogSink> instance = std::make_shared<EditorLogSink>();
            return instance;
        }

        void Drain(std::vector<Entry>& out)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& entry : m_pending)
                out.push_back(std::move(entry));
            m_pending.clear();
        }

      protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            spdlog::memory_buf_t formatted;
            formatter_->format(msg, formatted);

            std::string text = fmt::to_string(formatted);
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.pop_back();

            if (m_pending.size() >= MaxPending) m_pending.pop_front();
            m_pending.push_back({msg.level, std::move(text)});
        }

        void flush_() override {}

      private:
        std::deque<Entry> m_pending;
    };
} // namespace Wild
