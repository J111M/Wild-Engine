#pragma once

#include "Renderer/Device.hpp"

#include "Renderer/RenderGraph/Blackboard.hpp"
#include "Renderer/Resources/Texture.hpp"

#include "Tools/Log.hpp"

#include <any>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>

// Class insipered by @Tygo Boons https://t-boons.github.io/Rendergraph/

namespace Wild
{
#define GRAPHRESOURCE_LIFETIME 5

    class TransientResourceCache : public NonCopyable
    {
      public:
        TransientResourceCache() {}
        ~TransientResourceCache();

        void UpdateLifetimes();
        void Flush();

        bool HasValue(const std::string& key) const;

        uint32_t NumResources() const { return static_cast<uint32_t>(m_resources.size()); }

        Texture& GetOrCreateTexture(const std::string& key, const TextureDesc& desc);

      private:
        template <typename AnyType> void AddResource(const std::string& key, AnyType *args);

        void DeallocateResources();

        std::unordered_map<std::string, std::any> m_resources;
        std::unordered_map<std::string, uint32_t> m_lifetimes;
    };

    inline TransientResourceCache::~TransientResourceCache()
    {
        for (auto& resource : m_resources)
        {
            resource.second.reset();
        }
    }

    inline void TransientResourceCache::UpdateLifetimes()
    {
        for (auto& lifetime : m_lifetimes)
        {
            lifetime.second--;
        }

        DeallocateResources();
    }

    inline void TransientResourceCache::Flush()
    {
        // Delete all resources
        for (auto it = m_resources.begin(); it != m_resources.end();)
        {
            bool shouldKeep = false;

            if (auto texture = std::any_cast<std::shared_ptr<Texture>>(&it->second))
            {
                shouldKeep = (*texture)->GetDesc().shouldStayInCache;
            }

            if (shouldKeep) { it++; }
            else
            {
                m_lifetimes.erase(it->first);
                it = m_resources.erase(it);
            }
        }

        WD_INFO("Deallocated/Flushed all transient resources.");
    }

    inline bool TransientResourceCache::HasValue(const std::string& key) const
    {
        return m_lifetimes.find(key) != m_lifetimes.end();
    }

    inline Texture& TransientResourceCache::GetOrCreateTexture(const std::string& key, const TextureDesc& desc)
    {
        auto it = m_resources.find(key);
        if (it == m_resources.end())
        {
            Texture *newResource = new Texture(desc);

            AddResource<Texture>(key, newResource);
            return *newResource;
        }

        // Get an existing resource.
        std::shared_ptr<Texture> out = std::any_cast<std::shared_ptr<Texture>>(it->second);

        // Reset the resource's lifetime.
        m_lifetimes[key] = GRAPHRESOURCE_LIFETIME;

        if (out) assert(out);

        return *out.get();
    }

    inline void TransientResourceCache::DeallocateResources()
    {
        // Caching the resources that are due for deletion, this is done to not brake the loop
        std::queue<std::string> resourcesToDeallocate;

        for (auto& lifetime : m_lifetimes)
        {
            // Lifetime has ended deallocate the resource
            const std::string& key = lifetime.first;

            // Lifetime has ended add to the deallocate que
            if (lifetime.second == 0) { resourcesToDeallocate.push(key); }
        }

        // Delete everything in the deallocate queue
        while (!resourcesToDeallocate.empty())
        {
            const std::string key = resourcesToDeallocate.front();
            resourcesToDeallocate.pop();

            std::any& resource = m_resources.at(key);
            resource.reset();

            m_lifetimes.erase(key);
            m_resources.erase(key);

            WD_INFO("Deallocated Transient resource: {0}", key.c_str());
        }
    }

    template <typename AnyType> inline void TransientResourceCache::AddResource(const std::string& key, AnyType *args)
    {
        std::stringstream ss{};
        ss << "Wild::AllocResource -> " << key;
        // TODO profile

        // Initialize a new resource.
        std::shared_ptr<AnyType> newResource = std::shared_ptr<AnyType>(args);
        m_resources[key] = newResource;
        m_lifetimes[key] = GRAPHRESOURCE_LIFETIME;

        WD_INFO("Allocated new Transient resource: {0}", key);
    }
} // namespace Wild
