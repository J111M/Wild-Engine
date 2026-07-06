#include "Systems/SceneManager.hpp"

#include "Core/Engine.hpp"
#include "Core/Transform.hpp"

#include <algorithm>

namespace Wild
{
    namespace
    {
        void CollectDescendants(entt::registry& registry, Entity entity, std::vector<Entity>& toDestroy)
        {
            auto& transform = registry.get<Transform>(entity);
            for (auto child : transform.GetChildren())
                CollectDescendants(registry, child, toDestroy);
            toDestroy.push_back(entity);
        }

        void RemoveAccelerationInstances(entt::registry& registry, const std::vector<Entity>& toDestroy)
        {
            std::vector<uint32_t> tlasIndices;
            for (auto e : toDestroy)
            {
                if (auto tlasIndex = registry.get<Transform>(e).GetTlasIndex()) tlasIndices.push_back(*tlasIndex);
            }

            if (tlasIndices.empty()) return;

            std::sort(tlasIndices.begin(), tlasIndices.end(), std::greater<uint32_t>());
            auto as = engine.GetAccelerationStructureManager();
            for (auto index : tlasIndices)
                as->RemoveInstance(index);
        }
    } // namespace

    void DestroySceneObjectsRecursive(entt::registry& registry)
    {
        std::vector<Entity> toDestroy;
        for (auto entity : registry.view<SceneObject>())
            CollectDescendants(registry, entity, toDestroy);

        if (toDestroy.empty()) return;

        RemoveAccelerationInstances(registry, toDestroy);

        engine.GetGfxContext()->Flush();

        for (auto e : toDestroy)
        {
            if (registry.valid(e)) registry.destroy(e);
        }
    }

    void DestroyEntityRecursive(entt::registry& registry, entt::entity entity)
    {
        if (!registry.valid(entity)) return;

        std::vector<Entity> toDestroy;
        CollectDescendants(registry, entity, toDestroy);

        RemoveAccelerationInstances(registry, toDestroy);
        engine.GetGfxContext()->Flush();

        for (auto e : toDestroy)
        {
            if (registry.valid(e)) registry.destroy(e);
        }
    }

    void SceneManager::AddScene(const std::string& sceneName, std::function<void()> scene) { m_scenes[sceneName] = scene; }

    void SceneManager::LoadScene(const std::string& sceneName)
    {
        UnloadPreviousScene();

        m_currentScene = sceneName;

        // Iterate over the map and find the scene than execute it's captured data
        auto it = m_scenes.find(sceneName);
        if (it != m_scenes.end()) { it->second(); }
    }

    void SceneManager::UnloadPreviousScene()
    {
        if (m_currentScene.empty()) return;

        DestroySceneObjectsRecursive(engine.GetECS()->GetRegistry());
    }
} // namespace Wild
