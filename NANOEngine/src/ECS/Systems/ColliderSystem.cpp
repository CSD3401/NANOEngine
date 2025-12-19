#include "ColliderSystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../../Physics/PhysicsManager.hpp"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>


namespace NE::ECS::Systems {

	ColliderSystem::ColliderSystem(ComponentManager* cm) : m_componentManager(cm) { }

	void ColliderSystem::OnEntityAdded(Entity) {
	}

	void ColliderSystem::OnEntityRemoved(Entity entity) {

	}

	void ColliderSystem::Init() {

	}

	void ColliderSystem::Update(double) {

	}

	void ColliderSystem::Exit() {

	}

}