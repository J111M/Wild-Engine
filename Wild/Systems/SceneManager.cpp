#include "Systems/SceneManager.hpp"

namespace Wild
{
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

        auto& registry = engine.GetECS()->GetRegistry();
        auto view = registry.view<SceneObject>();

        std::vector<entt::entity> toDestroy;
        for (auto entity : view)
        {
            auto& transform = registry.get<Transform>(entity);
            for (auto child : transform.GetChildren())
                toDestroy.push_back(child);
            toDestroy.push_back(entity);
        }

        for (auto e : toDestroy)
        {
            if (registry.valid(e)) registry.destroy(e);
        }
    }
} // namespace Wild
