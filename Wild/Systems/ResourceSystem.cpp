#include "Systems/ResourceSystem.hpp"

#include "Renderer/Resources/Texture.hpp"
#include "Renderer/Resources/Mesh.hpp"

namespace Wild
{
    template <typename Resource>
    template <typename... Args>
    inline std::shared_ptr<Resource> ResourceSystem<Resource>::GetOrCreateResource(std::string key, Args&&... args)
    {
        auto it = m_resources.find(key);
        if (it != m_resources.end())
        {
            if (auto resource = it->second.lock()) return resource;
        }

        auto resource = std::make_shared<Resource>(std::forward<Args>(args)...);
        m_resources[key] = resource;
        return resource;
    }

    // Explicit template types
    template class ResourceSystem<Texture>;
    template class ResourceSystem<Mesh>;
}