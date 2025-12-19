#pragma once

#include "Tools/Log.hpp"

#include <unordered_map>
#include <string>
#include <any>

// Class insipered by @Tygo Boons https://t-boons.github.io/Rendergraph/

namespace Wild {
	class Blackboard : public NonCopyable
	{
        template <typename ResourceType>
        void Register(ResourceType* value, const std::string& key);

        template <typename ResourceType>
        ResourceType& Get(const std::string& key) const;

        template <typename ResourceType>
        bool TryGet(const std::string& key, ResourceType*& out) const;

        template <typename ResourceType>
        void Replace(ResourceType* value, const std::string& key);

        template <typename ResourceType>
        void RegisterOrReplace(ResourceType* value, const std::string& key);

        void Erase(const std::string& key);

        // Iterate over the unordered map to check if the key is in the map
        bool HasValue(const std::string& key) const;

        uint32_t NumResources() const { return static_cast<uint32_t>(m_blackboard.size()); }

	private:
		inline static std::unordered_map<std::string, std::any> m_blackboard;
	};

    template<typename ResourceType>
    inline void Blackboard::Register(ResourceType* value, const std::string& key)
    {
        if (HasValue(key)) 
        {
            WD_INFO("Trying to register a duplicate key: %s", key.c_str());
            return;
        }
           
        m_blackboard[key] = value;
    }

    template<typename ResourceType>
    inline ResourceType& Blackboard::Get(const std::string& key) const
    {
        auto it = m_blackboard.find(key);
        if (it != m_blackboard.end())
        {
            ResourceType* ptr = nullptr;

            // Converts std::any to Resource type
            ptr = std::any_cast<ResourceType*>(it->second);
            return *ptr;
        }

        WD_FATAL("Blackboard does not have value with key: {}", key.c_str());
        ResourceType* errorDummy = nullptr;
        return *errorDummy;
    }

    template<typename ResourceType>
    inline bool Blackboard::TryGet(const std::string& key, ResourceType*& out) const
    {
        if (!HasValue(key))
        {
            return false;
        }

        out = &Get<ResourceType>(key);
        return true;
    }

    template<typename ResourceType>
    inline void Blackboard::Replace(ResourceType* value, const std::string& key)
    {
        if (!HasValue(key))
        {
            WD_INFO("Blackboard doesn't have value with key: %s", key.c_str());
            return;
        }

        m_blackboard[key] = value;
    }

    template<typename ResourceType>
    inline void Blackboard::RegisterOrReplace(ResourceType* value, const std::string& key)
    {
        if (HasValue(key)) { Replace<ResourceType>(value, key); }
        else { Register<ResourceType>(value, key); }
    }

    inline void Blackboard::Erase(const std::string& key)
    {
        if (!HasValue(key))
        {
            WD_INFO("Blackboard does not contain key: %s", key.c_str());
            return;
        }

        m_blackboard.erase(m_blackboard.find(key));
    }

    inline bool Blackboard::HasValue(const std::string& key) const {
        auto it = m_blackboard.find(key);
        if (it != m_blackboard.end()) { return true; }
        return false;
    }
}

