#include "Core/ECS.hpp"

namespace Wild
{
    void EntityComponentSystem::DestroyEntity(Entity entity) { m_registry.destroy(entity); }
} // namespace Wild
