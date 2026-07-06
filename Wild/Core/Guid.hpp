#pragma once

#include <cstdint>
#include <limits>
#include <random>

namespace Wild
{
    // Stable cross-run identity for an entity, since entt::entity handles are
    // not stable across save/load. 0 is reserved as the invalid/unassigned value.
    struct Guid
    {
        uint64_t value = 0;
    };

    inline uint64_t GenerateGuid()
    {
        static std::mt19937_64 engine{std::random_device{}()};
        static std::uniform_int_distribution<uint64_t> dist(1, std::numeric_limits<uint64_t>::max());
        return dist(engine);
    }
} // namespace Wild
