#include "pch.h"
#include "ColliderSystem.hpp"
#include "ECS/Components/Collider.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/CharacterController.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/EntityMeta.hpp"
#include "Physics/PhysicsManager.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"


namespace NE::ECS::Systems {

	ColliderSystem::ColliderSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr)
	: m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) { }

	void ColliderSystem::OnEntityAdded(Entity e) {
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		auto& col = m_componentManager->GetComponent<Component::Collider>(e);
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);

		if (col.luid == 0) {
			col.luid = Core::LUIDGenerator::Generate("co");

			if (m_componentManager->HasComponent<Component::Renderer>(e)) {
				if (col.type == Component::Collider::ColliderType::Box) {
					auto& renderer = m_componentManager->GetComponent<Component::Renderer>(e);
					if (renderer.model) {
						Graphics::AABB bounds = renderer.model->meshes[renderer.subMeshIndex].localAABB;
						auto& boxData = std::get<Component::Collider::BoxColliderData>(col.data);
						boxData.halfExtents = (bounds.max - bounds.min) * 0.5f;
					}
				}
			}
		}

		m_luidRegistry->Register(col.luid, &col, e);

		Physics::PhysicsManager::GetInstance().CreateOrUpdateShape(e, meta.luid, col);
	}

	void ColliderSystem::OnEntityRemoved(Entity e) {
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		auto& col = m_componentManager->GetComponent<Component::Collider>(e);

		m_luidRegistry->Unregister(col.luid);

		Physics::PhysicsManager::GetInstance().RemoveShape(meta.luid);
	}

	void ColliderSystem::OnEntityActive(Entity e) {
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		Physics::PhysicsManager::GetInstance().UpdateBodyState(meta.luid, true);
	}

	void ColliderSystem::OnEntityInactive(Entity e) {
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		Physics::PhysicsManager::GetInstance().UpdateBodyState(meta.luid, false);
	}

	void ColliderSystem::Init() {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			auto& col = m_componentManager->GetComponent<Component::Collider>(e);

			bool hasColliderOnly = !(m_componentManager->HasComponent<Component::Rigidbody>(e) || 
				m_componentManager->HasComponent<Component::CharacterController>(e));

			if (hasColliderOnly)
				Physics::PhysicsManager::GetInstance().CreateBody(e, meta.luid, t, col, static_cast<uint8_t>(m_entityManager->GetLayer(e)));
		}
	}

	void ColliderSystem::Update(double) {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			//auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			auto& col = m_componentManager->GetComponent<Component::Collider>(e);

			if (col.isDirty) {
				Physics::PhysicsManager::GetInstance().CreateOrUpdateShape(e, meta.luid, col);
				col.isDirty = false;
			}
		}
	}

	void ColliderSystem::Exit() {

	}

}