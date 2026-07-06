#pragma once

#include "Core/ECS.hpp"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace Wild
{
    class SceneSerializer : public NonCopyable
    {
      public:
        SceneSerializer() {};
        ~SceneSerializer() {};

        bool Serialize(const std::filesystem::path& path);
        bool Deserialize(const std::filesystem::path& path);

        // In memory variants, used by UndoSystem for fast checkpointing without touching disk
        std::string SerializeToString();
        bool DeserializeFromString(const std::string& jsonText);

        // Destroys every SceneObject tagged entity
        void ClearScene();

        std::string CopyEntitySubtree(Entity root);
        Entity PasteEntitySubtree(const std::string& jsonText, Entity newParent);

      private:
        bool ShouldSerializeAsRoot(Entity e);
        nlohmann::json SerializeEntity(Entity e);
        nlohmann::json BuildSceneJson();
        bool LoadSceneJson(const nlohmann::json& root);
        bool NodeMatchesEntity(Entity e, const nlohmann::json& node);
        void UpdateEntityInPlace(Entity e, const nlohmann::json& node);

        Entity CreateEntityFromJson(const nlohmann::json& node);
    };
} // namespace Wild
