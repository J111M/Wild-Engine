#pragma once

#include <entt/entity/registry.hpp>
#include <entt/entt.hpp>

#include <vector>

namespace Wild {
	using Entity = entt::entity;

	class EntityComponentSystem {

	public:
		Entity CreateEntity() { return m_registry.create(); }
		void DestroyEntity(Entity entity);

		template<typename T, typename... Args>
		T& AddComponent(Entity entity, Args&&... args) {
			return m_registry.emplace<T>(entity, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent(Entity entity) {
			return m_registry.get<T>(entity);
		}

		template<typename T>
		bool HasComponent(Entity entity) const {
			return m_registry.any_of<T>(entity);
		}

		template<typename T>
		void RemoveComponent(Entity entity) {
			m_registry.remove<T>(entity);
		}

		template<typename T>
		std::vector<Entity> View() {
			std::vector<entt::entity> entities;
			auto view = m_registry.view<T>();
			for (auto entity : view) {
				entities.push_back(entity);
			}
			return entities;
		}

		entt::registry& GetRegistry() {
			return m_registry;
		}
	private:
		entt::registry m_registry;
	};
}