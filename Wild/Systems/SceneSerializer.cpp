#include "Systems/SceneSerializer.hpp"

#include "Core/Engine.hpp"
#include "Core/Guid.hpp"
#include "Core/Transform.hpp"

#include "Renderer/Resources/LightTypes.hpp"
#include "Renderer/Resources/Model.hpp"

#include "Systems/SceneManager.hpp"

#include "Tools/Log.hpp"

#include <nlohmann/json.hpp>

#include <fstream>

// Stored the scene data into a json file for loading
namespace Wild
{
    namespace
    {
        nlohmann::json ToJson(const glm::vec3& v) { return {{"x", v.x}, {"y", v.y}, {"z", v.z}}; }
        nlohmann::json ToJson(const glm::vec4& v) { return {{"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w}}; }
        nlohmann::json ToJson(const glm::quat& q) { return {{"x", q.x}, {"y", q.y}, {"z", q.z}, {"w", q.w}}; }

        glm::vec3 Vec3FromJson(const nlohmann::json& j) { return glm::vec3(j["x"].get<float>(), j["y"].get<float>(), j["z"].get<float>()); }
        glm::vec4 Vec4FromJson(const nlohmann::json& j)
        {
            return glm::vec4(j["x"].get<float>(), j["y"].get<float>(), j["z"].get<float>(), j["w"].get<float>());
        }
        glm::quat QuatFromJson(const nlohmann::json& j)
        {
            return glm::quat(j["w"].get<float>(), j["x"].get<float>(), j["y"].get<float>(), j["z"].get<float>());
        }

        constexpr int kSceneFileVersion = 1;
    } // namespace

    void SceneSerializer::ClearScene() { DestroySceneObjectsRecursive(engine.GetECS()->GetRegistry()); }

    bool SceneSerializer::ShouldSerializeAsRoot(Entity e)
    {
        auto& registry = engine.GetECS()->GetRegistry();
        if (!registry.any_of<SceneObject>(e)) return false;

        auto& transform = registry.get<Transform>(e);
        return !transform.HasParent();
    }

    nlohmann::json SceneSerializer::SerializeEntity(Entity e)
    {
        auto ecs = engine.GetECS();
        auto& registry = ecs->GetRegistry();

        if (!ecs->HasComponent<Guid>(e)) ecs->AddComponent<Guid>(e, GenerateGuid());

        auto& transform = ecs->GetComponent<Transform>(e);
        auto& guid = ecs->GetComponent<Guid>(e);

        nlohmann::json node;
        node["guid"] = guid.value;
        node["name"] = transform.Name;
        if (transform.HasParent()) node["parentGuid"] = ecs->GetComponent<Guid>(transform.GetParent()).value;
        else node["parentGuid"] = nullptr;
        node["transform"] = {
            {"position", ToJson(transform.GetPosition())},
            {"rotation", ToJson(transform.GetRotation())},
            {"scale", ToJson(transform.GetScale())},
        };

        if (ecs->HasComponent<Model>(e))
        {
            node["model"] = {{"sourcePath", ecs->GetComponent<Model>(e).GetSourcePath()}};
            return node;
        }

        if (ecs->HasComponent<PointLight>(e))
        {
            auto& light = ecs->GetComponent<PointLight>(e);
            node["pointLight"] = {{"colorIntensity", ToJson(light.colorIntensity)}, {"position", ToJson(light.position)}};
        }
        else if (ecs->HasComponent<DirectionalLight>(e))
        {
            auto& light = ecs->GetComponent<DirectionalLight>(e);
            node["directionalLight"] = {{"colorIntensity", ToJson(light.colorIntensity)}, {"direction", ToJson(light.direction)}};
        }

        return node;
    }

    nlohmann::json SceneSerializer::BuildSceneJson()
    {
        auto& registry = engine.GetECS()->GetRegistry();

        nlohmann::json entities = nlohmann::json::array();

        // Walk each SceneObject root, recursing into children, but stopping at
        // Model roots since their subtree is regenerated on load
        std::vector<Entity> stack;
        for (auto e : registry.view<Transform>())
        {
            if (ShouldSerializeAsRoot(e)) stack.push_back(e);
        }

        while (!stack.empty())
        {
            Entity e = stack.back();
            stack.pop_back();

            entities.push_back(SerializeEntity(e));

            if (!registry.any_of<Model>(e))
            {
                for (auto child : registry.get<Transform>(e).GetChildren())
                    stack.push_back(child);
            }
        }

        return {{"version", kSceneFileVersion}, {"entities", entities}};
    }

    bool SceneSerializer::Serialize(const std::filesystem::path& path)
    {
        nlohmann::json root = BuildSceneJson();

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ofstream file(path);
        if (!file.is_open())
        {
            WD_WARN("Failed to open scene file for writing: {}", path.string());
            return false;
        }

        file << root.dump(2);
        WD_INFO("Scene saved: {}", path.string());
        return true;
    }

    std::string SceneSerializer::SerializeToString() { return BuildSceneJson().dump(); }

    bool SceneSerializer::NodeMatchesEntity(Entity e, const nlohmann::json& node)
    {
        auto ecs = engine.GetECS();

        if (node.contains("model"))
        {
            return ecs->HasComponent<Model>(e) &&
                   ecs->GetComponent<Model>(e).GetSourcePath() == node["model"]["sourcePath"].get<std::string>();
        }
        if (node.contains("pointLight")) return ecs->HasComponent<PointLight>(e);
        if (node.contains("directionalLight")) return ecs->HasComponent<DirectionalLight>(e);

        return !ecs->HasComponent<Model>(e) && !ecs->HasComponent<PointLight>(e) && !ecs->HasComponent<DirectionalLight>(e);
    }

    void SceneSerializer::UpdateEntityInPlace(Entity e, const nlohmann::json& node)
    {
        auto ecs = engine.GetECS();
        auto& transform = ecs->GetComponent<Transform>(e);
        transform.Name = node.value("name", std::string());

        const auto& t = node["transform"];
        transform.SetPosition(Vec3FromJson(t["position"]));
        glm::quat rotation = QuatFromJson(t["rotation"]);
        transform.SetRotation(rotation);
        transform.SetScale(Vec3FromJson(t["scale"]));

        if (node.contains("pointLight"))
        {
            auto& light = ecs->GetComponent<PointLight>(e);
            light.colorIntensity = Vec4FromJson(node["pointLight"]["colorIntensity"]);
            light.position = Vec3FromJson(node["pointLight"]["position"]);
        }
        else if (node.contains("directionalLight"))
        {
            auto& light = ecs->GetComponent<DirectionalLight>(e);
            light.colorIntensity = Vec4FromJson(node["directionalLight"]["colorIntensity"]);
            light.direction = Vec3FromJson(node["directionalLight"]["direction"]);
        }
    }

    Entity SceneSerializer::CreateEntityFromJson(const nlohmann::json& node)
    {
        auto ecs = engine.GetECS();

        Entity e = ecs->CreateEntity();

        uint64_t guid = node["guid"].get<uint64_t>();
        ecs->AddComponent<Guid>(e, guid);
        ecs->AddComponent<SceneObject>(e);

        auto& transform = ecs->AddComponent<Transform>(e, e);
        transform.Name = node.value("name", std::string());

        const auto& t = node["transform"];
        transform.SetPosition(Vec3FromJson(t["position"]));
        glm::quat rotation = QuatFromJson(t["rotation"]);
        transform.SetRotation(rotation);
        transform.SetScale(Vec3FromJson(t["scale"]));

        if (node.contains("model"))
        {
            // Re-runs the full glTF load, reusing cached GPU resources by path
            ecs->AddComponent<Model>(e, std::filesystem::path(node["model"]["sourcePath"].get<std::string>()), e);
        }
        else if (node.contains("pointLight"))
        {
            auto& light = ecs->AddComponent<PointLight>(e);
            light.colorIntensity = Vec4FromJson(node["pointLight"]["colorIntensity"]);
            light.position = Vec3FromJson(node["pointLight"]["position"]);
        }
        else if (node.contains("directionalLight"))
        {
            auto& light = ecs->AddComponent<DirectionalLight>(e);
            light.colorIntensity = Vec4FromJson(node["directionalLight"]["colorIntensity"]);
            light.direction = Vec3FromJson(node["directionalLight"]["direction"]);
        }

        return e;
    }

    bool SceneSerializer::LoadSceneJson(const nlohmann::json& root)
    {
        if (!root.contains("version") || root["version"].get<int>() != kSceneFileVersion)
        {
            WD_WARN("Scene data has an unsupported version");
            return false;
        }

        auto ecs = engine.GetECS();
        auto& registry = ecs->GetRegistry();

        // Snapshot everything currently alive by guid, lazily assigning one to anything that's missing it
        std::unordered_map<uint64_t, Entity> currentByGuid;
        for (auto e : registry.view<SceneObject>())
        {
            if (!ecs->HasComponent<Guid>(e)) ecs->AddComponent<Guid>(e, GenerateGuid());
            currentByGuid[ecs->GetComponent<Guid>(e).value] = e;
        }

        std::unordered_map<uint64_t, Entity> guidToEntity;
        std::unordered_map<uint64_t, bool> kept;

        for (const auto& node : root["entities"])
        {
            uint64_t guid = node["guid"].get<uint64_t>();
            auto it = currentByGuid.find(guid);

            bool canKeep = it != currentByGuid.end() && NodeMatchesEntity(it->second, node);
            kept[guid] = canKeep;

            if (canKeep)
            {
                UpdateEntityInPlace(it->second, node);
                guidToEntity[guid] = it->second;
            }
        }

        for (auto& [guid, entity] : currentByGuid)
        {
            if (!kept[guid] && registry.valid(entity)) DestroyEntityRecursive(registry, entity);
        }

        for (const auto& node : root["entities"])
        {
            uint64_t guid = node["guid"].get<uint64_t>();
            if (!kept[guid]) guidToEntity[guid] = CreateEntityFromJson(node);
        }

        for (const auto& node : root["entities"])
        {
            if (node["parentGuid"].is_null()) continue;

            auto childIt = guidToEntity.find(node["guid"].get<uint64_t>());
            auto parentIt = guidToEntity.find(node["parentGuid"].get<uint64_t>());
            if (childIt == guidToEntity.end() || parentIt == guidToEntity.end())
            {
                WD_WARN("Scene data references an unknown parent guid, leaving entity unparented");
                continue;
            }

            auto& transform = ecs->GetComponent<Transform>(childIt->second);
            if (transform.GetParent() != parentIt->second) transform.SetParent(parentIt->second);
        }

        return true;
    }

    bool SceneSerializer::Deserialize(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            WD_WARN("Failed to open scene file for reading: {}", path.string());
            return false;
        }

        nlohmann::json root;
        try
        {
            file >> root;
        }
        catch (const nlohmann::json::exception& ex)
        {
            WD_WARN("Failed to parse scene file {}: {}", path.string(), ex.what());
            return false;
        }

        if (!LoadSceneJson(root)) return false;

        WD_INFO("Scene loaded: {}", path.string());
        return true;
    }

    bool SceneSerializer::DeserializeFromString(const std::string& jsonText)
    {
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(jsonText);
        }
        catch (const nlohmann::json::exception& ex)
        {
            WD_WARN("Failed to parse in-memory scene snapshot: {}", ex.what());
            return false;
        }

        return LoadSceneJson(root);
    }
} // namespace Wild
