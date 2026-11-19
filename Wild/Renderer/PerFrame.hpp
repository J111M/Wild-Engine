#pragma once

#include <array>
#include <cstdint>

namespace Wild
{
    constexpr int BACK_BUFFER_COUNT = 2;

    // Index of the back buffer the current frame records into
    uint32_t GetFrameIndex();

    // One copy of T per frame in flight.
    template <typename T> class PerFrame
    {
      public:
        // This frame's copy
        T& Get() { return m_items[GetFrameIndex()]; }
        const T& Get() const { return m_items[GetFrameIndex()]; }

        // Every copy, for creating or resetting them all at once
        auto begin() { return m_items.begin(); }
        auto end() { return m_items.end(); }

      private:
        std::array<T, BACK_BUFFER_COUNT> m_items{};
    };
} // namespace Wild
