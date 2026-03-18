#pragma once

#include <memory>
#include <unordered_map>

namespace Wild
{
    // TODO instead of having a resource system per resource make a resource base class that resources can derive from
    template <typename Resource> class ResourceSystem : public NonCopyable
    {
      public:
        ResourceSystem() {};
        ~ResourceSystem() {};

        template <typename... Args> std::shared_ptr<Resource> GetOrCreateResource(const std::string& key, Args&&... args);
        // template <typename... Args> std::shared_ptr<Resource> CreateResource(const std::string& key, Args&&... args);

        bool HasResource(const std::string key);
        std::shared_ptr<Resource> GetResource(const std::string& key);

        void RemoveResource(const std::string& key);

      private:
        std::unordered_map<std::string, std::weak_ptr<Resource>> m_resources{};
    };

    /// <summary>
    /// Inline declerations for templated functions
    /// Telling the compiler to only pick 1 definition of the function
    /// </summary>
    template <typename Resource>
    template <typename... Args>
    std::shared_ptr<Resource> inline ResourceSystem<Resource>::GetOrCreateResource(const std::string& key, Args&&... args)
    {
        if (auto it = m_resources.find(key); it != m_resources.end())
        {
            if (auto resource = it->second.lock()) return resource;
        }

        auto resource = std::make_shared<Resource>(std::forward<Args>(args)...);
        m_resources[key] = resource;
        return resource;
    }

    template <typename Resource> inline bool ResourceSystem<Resource>::HasResource(const std::string key)
    {
        // TODO when moving to cpp 20 use contains function
        auto it = m_resources.find(key);
        if (it == m_resources.end()) return false;
        return !it->second.expired();
    }

    template <typename Resource> inline std::shared_ptr<Resource> ResourceSystem<Resource>::GetResource(const std::string& key)
    {
        if (auto it = m_resources.find(key); it != m_resources.end())
        {
            if (auto resource = it->second.lock()) return resource;
        }

        return nullptr;
    }

    template <typename Resource> inline void ResourceSystem<Resource>::RemoveResource(const std::string& key)
    {
        m_resources.erase(key);
    }

} // namespace Wild
