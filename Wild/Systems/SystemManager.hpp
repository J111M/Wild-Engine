#pragma once

#include <cassert>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

namespace Wild
{

    class System
    {
      public:
        virtual ~System() = default;
    };

    class SystemManager
    {
      public:
        // Adds new system to the registry.
        template <typename T, typename... Args> T& AddSystem(Args&&... args)
        {
            static_assert(std::is_base_of_v<System, T>, "T must derive from System");
            auto system = std::make_unique<T>(std::forward<Args>(args)...);
            T& ref = *system;
            auto [it, inserted] = m_systems.emplace(std::type_index(typeid(T)), std::move(system));
            assert(inserted && "system already registered");
            return ref;
        }

        // Use when the system must exist.
        template <typename T> T& GetSystem()
        {
            auto it = m_systems.find(std::type_index(typeid(T)));
            assert(it != m_systems.end() && "system not registered");
            return *static_cast<T*>(it->second.get());
        }

        // Use when the system could be optional.
        template <typename T> T* TryGetSystem()
        {
            auto it = m_systems.find(std::type_index(typeid(T)));
            return it == m_systems.end() ? nullptr : static_cast<T*>(it->second.get());
        }

      private:
        std::unordered_map<std::type_index, std::unique_ptr<System>> m_systems;
    };

} // namespace Wild
