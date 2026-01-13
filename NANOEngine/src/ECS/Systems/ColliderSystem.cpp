#include "ColliderSystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../Components/EntityMeta.hpp"
#include "Physics/PhysicsManager.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"


namespace NE::ECS::Systems {

	ColliderSystem::ColliderSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr)
	: m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) { }

	void ColliderSystem::OnEntityAdded(Entity e) {
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		auto& col = m_componentManager->GetComponent<Component::Collider>(e);

		// Generate LUID if not set
		if (col.luid == 0)
			col.luid = Core::LUIDGenerator::Generate("co");

		// Register with LUID registry
		m_luidRegistry->Register(col.luid, &col, e);

		// Existing physics shape creation
		Physics::PhysicsManager::GetInstance().CreateOrUpdateShape(meta.luid, col);
	}

	void ColliderSystem::OnEntityRemoved(Entity e) {
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		auto& col = m_componentManager->GetComponent<Component::Collider>(e);

		// Unregister from LUID registry
		m_luidRegistry->Unregister(col.luid);

		// Existing physics shape removal
		Physics::PhysicsManager::GetInstance().RemoveShape(meta.luid);
	}

	void ColliderSystem::Init() {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			auto& col = m_componentManager->GetComponent<Component::Collider>(e);

			if (!m_componentManager->HasComponent<Component::Rigidbody>(e))
				Physics::PhysicsManager::GetInstance().CreateBody(e, meta.luid, t, col, static_cast<uint8_t>(m_entityManager->GetLayer(e)));
		}
	}

	void ColliderSystem::Update(double) {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			auto& col = m_componentManager->GetComponent<Component::Collider>(e);
			if (col.isDirty) {
				Physics::PhysicsManager::GetInstance().CreateOrUpdateShape(meta.luid, col);
				col.isDirty = false;
			}

			if (col.type != Component::Collider::ColliderType::Mesh)
				Physics::PhysicsManager::GetInstance().DrawShapeGizmo(meta.luid, t, col);
			//if (col.type != Component::Collider::ColliderType::Mesh)
			//	Physics::PhysicsManager::GetInstance().DrawShapeGizmo(meta.luid, t);
		}
	}

	void ColliderSystem::Exit() {

	}

}