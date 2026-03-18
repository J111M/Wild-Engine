#include "Systems/ResourceSystem.hpp"

#include "Renderer/Resources/Mesh.hpp"
#include "Renderer/Resources/Texture.hpp"

namespace Wild
{
    // Explicit template types
    template class ResourceSystem<Texture>;
    template class ResourceSystem<Mesh>;

   
} // namespace Wild
