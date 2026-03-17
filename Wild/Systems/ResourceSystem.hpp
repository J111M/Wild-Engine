#pragma once

#include <unordered_map>

namespace Wild
{
    template<typename Resource>
    class ResourceSystem
    {
      public:
        ResourceSystem();
        ~ResourceSystem();

        Resource GetOrCreateResource(std::string key);

      private: 
          std::unordered_map<std::string, Resource> m_resource{};
    };
}