#pragma once

#include <memory>
#include <unordered_map>

namespace Wild
{
    template<typename Resource> 
    class ResourceSystem : public NonCopyable
    {
      public:
        ResourceSystem();
        ~ResourceSystem();

        template <typename... Args> 
        std::shared_ptr<Resource> GetOrCreateResource(std::string key, Args&&... args);

        template <typename... Args>
        std::shared_ptr<Resource> CreateResource(std::string key, Args&&... args);

        std::shared_ptr<Resource> GetResource(std::string key);
        

      private: 
          std::unordered_map<std::string, Resource> m_resource{};
    };
 
} // namespace Wild