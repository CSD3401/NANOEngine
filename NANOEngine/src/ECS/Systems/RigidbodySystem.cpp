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
#include "../../EngineState.hpp"
#include "../../Core/Logger.hpp"
#include <algorithm>

// Not in used atm, handled by PhysicsSystem - RF

namespace NE::ECS::Systems {

	RigidbodySystem::RigidbodySystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void RigidbodySystem::OnEntityAdded(Entity entity)
	{
		// if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
		// 	return;

		// auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		// auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

		// // REMOVED: Auto-detection by name - too unreliable at creation time
		// // Users should set Motion Type manually in the Inspector instead

		// // Determine motion type from Rigidbody component
		// JPH::EMotionType motionType;
		
		// // Priority: Check motionType field first, then isStatic flag
		// switch (rb.motionType) {
		// case 0: 
		// 	motionType = JPH::EMotionType::Static;
		// 	rb.isStatic = true;  // Keep in sync
		// 	break;
		// case 1: 
		// 	motionType = JPH::EMotionType::Kinematic;
		// 	rb.isStatic = false;
		// 	break;
		// case 2: 
		// default: 
		// 	motionType = JPH::EMotionType::Dynamic;
		// 	rb.isStatic = false;
		// 	break;
		// }
		
		// // Log final decision
		// const char* motionTypeName = (motionType == JPH::EMotionType::Static) ? "Static (Layer 0)" :
		// 							 (motionType == JPH::EMotionType::Kinematic) ? "Kinematic (Layer 1)" :
		// 							 "Dynamic (Layer 1)";
		// printf("Creating physics body for entity %d as %s (motionType=%d)\n", 
		// 	   entity, motionTypeName, rb.motionType);

		// // Check if entity has a Collider component
		// if (m_componentManager->HasComponent < Component::Collider>(entity))
		// {
		// 	auto& collider = m_componentManager->GetComponent<Component::Collider>(entity);	

		// 	// Initialize previous values to current values
		// 	collider.previousShapeType = collider.shapeType;
		// 	collider.previousHalfExtents = collider.halfExtents;
		// 	collider.previousRadius = collider.radius;
		// 	collider.previousHeight = collider.height;

		// 	// Clear dirty flags (they might be true for new entities)
		// 	collider.isShapeDirty = false;
		// 	collider.isPropertiesDirty = false;

		// 	CreatePhysicsBodyFromComponent(entity, transform, rb, collider, motionType);
		// }
		// else
		// {
		// 	// fallback to default box shape
		// 	printf("No collider found - creating default box\n");
		// 	Math::Vec3 defaultSize(1.0f, 1.0f, 1.0f);
		// 	rb.bodyID = Physics::PhysicsManager::CreateBoxBody(
		// 		transform.position, 
		// 		transform.rotation, 
		// 		defaultSize, 
		// 		motionType);
		// }
		(void)entity;
	}

	void RigidbodySystem::OnEntityRemoved(Entity entity)
	{
		// if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
		// 	return;
		// auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		
		// if (rb.bodyID != 0) {
		// 	Physics::PhysicsManager::DestroyBody(rb.bodyID);
		// 	Physics::PhysicsManager::UnregisterEntityBody(entity);
		// }
		(void)entity;
	}

	void RigidbodySystem::CreatePhysicsBodyFromComponent(Entity entity, Component::Transform& transform, Component::Rigidbody& rb, Component::Collider& collider, JPH::EMotionType motionType)
	{
		// Math::Vec3 fullSize = {
		// 	collider.halfExtents.x * 2.0f,
		// 	collider.halfExtents.y * 2.0f,
		// 	collider.halfExtents.z * 2.0f
		// };

		// // Log which layer this body will be on
		// const char* motionTypeName = (motionType == JPH::EMotionType::Static) ? "Static (Layer 0)" :
		// 							 (motionType == JPH::EMotionType::Kinematic) ? "Kinematic (Layer 1)" :
		// 							 "Dynamic (Layer 1)";
		// printf("Creating physics body for entity %d as %s\n", entity, motionTypeName);

		// // Create appropriate shape
		// switch (collider.shapeType)
		// {
		// case Component::Collider::ShapeType::Box:
		// {
		// 	rb.bodyID = Physics::PhysicsManager::CreateBoxBody(
		// 		transform.position,
		// 		transform.rotation,
		// 		fullSize,
		// 		motionType
		// 	);
		// 	printf("Created BOX physics body with ID %d\n", rb.bodyID);
		// 	break;
		// }
		// case Component::Collider::ShapeType::Sphere:
		// {
		// 	rb.bodyID = Physics::PhysicsManager::CreateSphereBody(
		// 		transform.position,
		// 		transform.rotation,
		// 		collider.radius,
		// 		motionType
		// 	);
		// 	printf("Created SPHERE physics body with ID %d\n", rb.bodyID);
		// 	break;
		// }
		// case Component::Collider::ShapeType::Capsule:
		// {
		// 	rb.bodyID = Physics::PhysicsManager::CreateCapsuleBody(
		// 		transform.position,
		// 		transform.rotation,
		// 		collider.height,
		// 		collider.radius,
		// 		motionType
		// 	);
		// 	printf("Created CAPSULE physics body with ID %d\n", rb.bodyID);
		// 	break;
		// }
		// case Component::Collider::ShapeType::None:
		// {
		// 	// Destroy existing body if switching to None
		// 	if (rb.bodyID != 0) {
		// 		Physics::PhysicsManager::DestroyBody(rb.bodyID);
		// 		Physics::PhysicsManager::UnregisterEntityBody(entity);
		// 		rb.bodyID = 0;
		// 	}
		// 	printf("WARNING: No physics body - shape type is None\n");
		// 	break;
		// }
		// }

		// // CRITICAL: Register the entity-body mapping after creating the body
		// if (rb.bodyID != 0) {
		// 	Physics::PhysicsManager::RegisterEntityBody(entity, rb.bodyID);
		// 	printf("Registered entity %d with body ID %d\n", entity, rb.bodyID);
		// }
		(void)entity; // unused	
		(void)transform;
		(void)rb;
		(void)collider;
		(void)motionType;
	}

	void RigidbodySystem::CheckForColliderChanges()
	{
		const auto& entities = GetEntities();

		for (Entity e : entities) 
		{
			if (!m_componentManager->HasComponent<Component::Collider>(e)) 
			{
				continue;
			}

			auto& collider = m_componentManager->GetComponent<Component::Collider>(e);

			// Clamp values prevent negative size
			collider.halfExtents.x = std::max(0.1f, collider.halfExtents.x);
			collider.halfExtents.y = std::max(0.1f, collider.halfExtents.y);
			collider.halfExtents.z = std::max(0.1f, collider.halfExtents.z);
			collider.radius = std::max(0.1f, collider.radius);
			collider.height = std::max(0.1f, collider.height);

			bool needsRecreation = false;

			// Check shape type change
			if (collider.shapeType != collider.previousShapeType) {
				printf("Shape type changed for entity %d\n", e);
				needsRecreation = true;
				collider.previousShapeType = collider.shapeType;
			}
			// Check any property changes
			else if (collider.halfExtents != collider.previousHalfExtents ||
				collider.radius != collider.previousRadius ||
				collider.height != collider.previousHeight) {
				printf("Collider properties changed for entity %d\n", e);
				needsRecreation = true;

				// Update all previous values
				collider.previousHalfExtents = collider.halfExtents;
				collider.previousRadius = collider.radius;
				collider.previousHeight = collider.height;
			}

			if (needsRecreation) {
				RecreatePhysicsBody(e);
			}
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
			oldMotionType = Physics::PhysicsManager::GetMotionType(rb.bodyID);
			Physics::PhysicsManager::DestroyBody(rb.bodyID);
			rb.bodyID = 0; // Clear old ID
		}

		// Create new body with updated shape/properties
		CreatePhysicsBodyFromComponent(entity, transform, rb, collider, oldMotionType);
	}

	void RigidbodySystem::Init()
	{
	}

	void RigidbodySystem::Update(double dt)
	{
		// // Check for collider changes and recreate physics bodies if needed
		// CheckForColliderChanges();

		// if (NE::GetEngineState() == EngineState::Play) {
		// 	Physics::PhysicsManager::Update(static_cast<float>(dt));

		// 	const auto& entities = GetEntities();
		// 	for (Entity e : entities) {
		// 		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);
		// 		auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

		// 		Math::Vec3 pos;
		// 		Math::Vec3 rot;
		// 		Physics::PhysicsManager::GetTransform(rb.bodyID, pos, rot);

		// 		transform.position = pos;
		// 		transform.rotation = rot;
		// 		transform.isDirty = true;
		// 	}
		// } 
		// else {
		// 	// EDIT MODE: Synchronize editor changes to physics
		// 	const auto& entities = GetEntities();
		// 	for (Entity e : entities) {
		// 		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);
		// 		auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

		// 		// Sync transform changes
		// 		if (transform.isDirty) {
		// 			Physics::PhysicsManager::SetTransform(
		// 				rb.bodyID, transform.position, transform.rotation);
		// 		}

				
		// 		// Get current motion type from physics body
		// 		JPH::EMotionType currentMotionType = Physics::PhysicsManager::GetMotionType(rb.bodyID);
		// 		JPH::EMotionType desiredMotionType;
				
		// 		// Convert component motionType to Jolt motion type
		// 		switch (rb.motionType) {
		// 			case 0: desiredMotionType = JPH::EMotionType::Static; break;
		// 			case 1: desiredMotionType = JPH::EMotionType::Kinematic; break;
		// 			case 2: 
		// 			default: desiredMotionType = JPH::EMotionType::Dynamic; break;
		// 		}

		// 		// If motion type changed in Inspector, update physics body
		// 		if (currentMotionType != desiredMotionType) {
		// 			printf("RigidbodySystem: Motion type changed for entity %d: %d -> %d\n", 
		// 				 e, static_cast<int>(currentMotionType), static_cast<int>(desiredMotionType));
		// 			Physics::PhysicsManager::SetMotionType(rb.bodyID, desiredMotionType);
					
		// 			// Keep isStatic in sync
		// 			rb.isStatic = (desiredMotionType == JPH::EMotionType::Static);
		// 		}
		// 	}
		// }
		(void)dt;
	}

	void RigidbodySystem::Exit()
	{
	}

}