#include "ColliderSystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../Components/EntityMeta.hpp"
#include "Physics/PhysicsManager.hpp"
//#include <Jolt/Physics/Collision/Shape/BoxShape.h>
//#include <Jolt/Physics/Collision/Shape/SphereShape.h>
//#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>


namespace NE::ECS::Systems {

	ColliderSystem::ColliderSystem(ComponentManager* cm) : m_componentManager(cm) { }

	void ColliderSystem::OnEntityAdded(Entity e) {
		auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		auto& col = m_componentManager->GetComponent<Component::Collider>(e);
		Physics::PhysicsManager::GetInstance().CreateOrUpdateShape(meta.luid, col);
	}

	void ColliderSystem::OnEntityRemoved(Entity entity) {

	}

	void ColliderSystem::Init() {

	}

	void ColliderSystem::Update(double) {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& col = m_componentManager->GetComponent<Component::Collider>(e);
			if (col.isDirty) {
				Physics::PhysicsManager::GetInstance().CreateOrUpdateShape(e, col);
			}
		}
	}

	void ColliderSystem::Exit() {

	}

}