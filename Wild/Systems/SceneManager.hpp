#pragma once

#include <functional>
#include <unordered_map>

namespace Wild
{
    struct SceneObject
    {
        uint32_t id;
    };

    // TODO add jasos parsing for scene loading
    class SceneManager : public NonCopyable
    {
      public:
        SceneManager() {};
        ~SceneManager() {};

        void AddScene(const std::string& sceneName, std::function<void()> scene);
        void LoadScene(const std::string& sceneName);

        const inline std::vector<std::string> GetSceneNames() const
        {
            std::vector<std::string> keys;
            keys.reserve(m_scenes.size());
            for (const auto& [key, _] : m_scenes)
                keys.push_back(key);
            return keys;
        }

      private:
        void UnloadPreviousScene();

        std::string m_currentScene{};
        std::unordered_map<std::string, std::function<void()>> m_scenes{};
    };
} // namespace Wild
