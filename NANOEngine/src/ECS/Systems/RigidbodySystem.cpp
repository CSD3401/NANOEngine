#include "RigidbodySystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../Components/EntityMeta.hpp"
#include "../../Physics/PhysicsManager.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <algorithm>

// Not in used atm, handled by PhysicsSystem - RF

namespace NE::ECS::Systems {

	RigidbodySystem::RigidbodySystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void RigidbodySystem::OnEntityAdded(Entity entity) {

	}

	void RigidbodySystem::OnEntityRemoved(Entity entity) {

	}

	void RigidbodySystem::CreatePhysicsBodyFromComponent(Entity entity, Component::Transform& transform, Component::Rigidbody& rb, Component::Collider& collider, JPH::EMotionType motionType) {

	}

	void RigidbodySystem::CheckForColliderChanges() {
		const auto& entities = GetEntities();

		for (Entity e : entities) 
		{
			if (!m_componentManager->HasComponent<Component::Collider>(e)) 
			{
				continue;
			}

			//auto& collider = m_componentManager->GetComponent<Component::Collider>(e);

			//// Clamp values prevent negative size
			//collider.halfExtents.x = std::max(0.1f, collider.halfExtents.x);
			//collider.halfExtents.y = std::max(0.1f, collider.halfExtents.y);
			//collider.halfExtents.z = std::max(0.1f, collider.halfExtents.z);
			//collider.radius = std::max(0.1f, collider.radius);
			//collider.height = std::max(0.1f, collider.height);

			//bool needsRecreation = false;

			//// Check shape type change
			//if (collider.shapeType != collider.previousShapeType) {
			//	printf("Shape type changed for entity %d\n", e);
			//	needsRecreation = true;
			//	collider.previousShapeType = collider.shapeType;
			//}
			//// Check any property changes
			//else if (collider.halfExtents != collider.previousHalfExtents ||
			//	collider.radius != collider.previousRadius ||
			//	collider.height != collider.previousHeight) {
			//	printf("Collider properties changed for entity %d\n", e);
			//	needsRecreation = true;

			//	// Update all previous values
			//	collider.previousHalfExtents = collider.halfExtents;
			//	collider.previousRadius = collider.radius;
			//	collider.previousHeight = collider.height;
			//}

			//if (needsRecreation) {
			//	RecreatePhysicsBody(e);
			//}
		}
	}
	
	void RigidbodySystem::RecreatePhysicsBody(Entity entity)
	{
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
		auto& collider = m_componentManager->GetComponent<Component::Collider>(entity);

		// Store current motion type
		JPH::EMotionType oldMotionType = JPH::EMotionType::Dynamic;
		if (rb.bodyID != 0) {
			//oldMotionType = Physics::PhysicsManager::GetMotionType(rb.bodyID);
			//Physics::PhysicsManager::DestroyBody(rb.bodyID);
			rb.bodyID = 0; // Clear old ID
		}

		// Create new body with updated shape/properties
		CreatePhysicsBodyFromComponent(entity, transform, rb, collider, oldMotionType);
	}

	void RigidbodySystem::Init() {

	}

	void RigidbodySystem::Update(double) {

	}

	void RigidbodySystem::Exit() {

	}

}