#include "RigidbodySystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../../Physics/PhysicsManager.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Math/Math.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include "../../EngineState.hpp"
#include "../../Core/Logger.hpp"

namespace NE::ECS::Systems {

	RigidbodySystem::RigidbodySystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	// OLD
	//void RigidbodySystem::OnEntityAdded(Entity entity)
	//{
	//	//if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
	//	//	return;
	//	////auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
	//	////auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
	//	//Component::Collider* col = nullptr;
	//	//if (m_componentManager->HasComponent<Component::Collider>(entity))
	//	//	col = &m_componentManager->GetComponent<Component::Collider>(entity);
	//	////rb.bodyId = Physics::PhysicsManager::CreateBody(transform, col);

	//	if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
	//		return;
	//	auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
	//	auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

	//	JPH::Quat rotation = JPH::Quat::sEulerAngles({ JPH::DegreesToRadians(transform.rotation.x),
	//													JPH::DegreesToRadians(transform.rotation.y),
	//													JPH::DegreesToRadians(transform.rotation.z) });
	//	//JPH::BodyCreationSettings settings(Physics::PhysicsManager::s_shapeMap.at(entity).GetPtr(),
	//	//	{ transform.position.x, transform.position.y, transform.position.z }, rotation,
	//	//	rb.motionType, rb.motionType == 0U ? 0 : 1);
	//	//settings.mLinearVelocity = { rb.initialVelocity.x, rb.initialVelocity.y, rb.initialVelocity.z };

	//	//auto shapePtr = Physics::PhysicsManager::s_shapeMap.at(entity).GetPtr();
	//	//auto it = Physics::PhysicsManager::s_shapeMap.find(entity);
	//	//if (it == Physics::PhysicsManager::s_shapeMap.end())
	//	//	return; // collider shape not found
	//	//auto shapePtr = it->second.GetPtr();
	//	JPH::RVec3 position(transform.position.x, transform.position.y, transform.position.z);
	//	//JPH::Quat rotation = ...; // already created

	//	// Map your POD uint8_t to Jolt EMotionType:
	//	JPH::EMotionType joltMotionType = JPH::EMotionType::Dynamic;
	//	//switch (rb.motionType) {
	//	//case 0: joltMotionType = JPH::EMotionType::Static; break;
	//	//case 1: joltMotionType = JPH::EMotionType::Kinematic; break;
	//	//case 2: joltMotionType = JPH::EMotionType::Dynamic; break;
	//	//}

	//	JPH::ObjectLayer objectLayer = (rb.motionType == 0U ? 0 : 1);

	//	//JPH::BodyCreationSettings settings(shapePtr, position, rotation, joltMotionType, objectLayer);
	//	//settings.mLinearVelocity = { rb.initialVelocity.x, rb.initialVelocity.y, rb.initialVelocity.z };

	//	//rb.bodyID = Physics::PhysicsManager::CreateBody(settings);
	//	//JPH::BodyID id = Physics::PhysicsManager::CreateBody(settings);

	//	JPH::BodyCreationSettings boxSettings(new JPH::BoxShape({ 0.5f, 0.5f, 0.5f }), position, rotation, joltMotionType, objectLayer);
	//	rb.bodyID = Physics::PhysicsManager::CreateBody(boxSettings);


	//	printf("RigidbodySystem: Created body with ID %d for entity %d\n", rb.bodyID, entity);
	//}

	// RF
	void RigidbodySystem::OnEntityAdded(Entity entity)
	{
		if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
			return;

		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

		// Check if entity has a Collider component
		if (m_componentManager->HasComponent < Component::Collider>(entity))
		{
			auto& collider = m_componentManager->GetComponent<Component::Collider>(entity);	

			// Initialize previous values to current values
			collider.previousShapeType = collider.shapeType;
			collider.previousHalfExtents = collider.halfExtents;
			collider.previousRadius = collider.radius;
			collider.previousHeight = collider.height;

			// Clear dirty flags (they might be true for new entities)
			collider.isShapeDirty = false;
			collider.isPropertiesDirty = false;

			CreatePhysicsBodyFromComponent(entity, transform, rb, collider, JPH::EMotionType::Dynamic);
		}
		else
		{
			// fallback to default box shape
			// untested fallback code - RF
			printf("No collider found - creating default box\n");
			Math::Vec3 defaultSize(1.0f, 1.0f, 1.0f);
			rb.bodyID = Physics::PhysicsManager::CreateBoxBody(
				transform.position, 
				transform.rotation, 
				defaultSize, 
				JPH::EMotionType::Dynamic);
		}
	}

	void RigidbodySystem::OnEntityRemoved(Entity entity)
	{
		if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
			return;
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		
		if (rb.bodyID != 0) {
			Physics::PhysicsManager::DestroyBody(rb.bodyID);
			Physics::PhysicsManager::UnregisterEntityBody(entity);
		}
	}

	void RigidbodySystem::CreatePhysicsBodyFromComponent(Entity entity, Component::Transform& transform, Component::Rigidbody& rb, Component::Collider& collider, JPH::EMotionType motionType)
	{
		Math::Vec3 fullSize = {
			collider.halfExtents.x * 2.0f,
			collider.halfExtents.y * 2.0f,
			collider.halfExtents.z * 2.0f
		};

		// Create appropriate shape
		switch (collider.shapeType)
		{
		case Component::Collider::ShapeType::Box:
		{
			rb.bodyID = Physics::PhysicsManager::CreateBoxBody(
				transform.position,
				transform.rotation,
				fullSize,
				motionType
			);
			printf("Created BOX physics body\n");
			break;
		}
		case Component::Collider::ShapeType::Sphere:
		{
			rb.bodyID = Physics::PhysicsManager::CreateSphereBody(
				transform.position,
				transform.rotation,
				collider.radius,
				motionType
			);
			printf("Created SPHERE physics body\n");
			break;
		}
		case Component::Collider::ShapeType::Capsule:
		{
			rb.bodyID = Physics::PhysicsManager::CreateCapsuleBody(
				transform.position,
				transform.rotation,
				collider.height,
				collider.radius,
				motionType
			);
			printf("Created CAPSULE physics body\n");
			break;
		}
		case Component::Collider::ShapeType::None:
		{
			// Destroy existing body if switching to None
			if (rb.bodyID != 0) {
				Physics::PhysicsManager::DestroyBody(rb.bodyID);
				Physics::PhysicsManager::UnregisterEntityBody(entity);
				rb.bodyID = 0;
			}
			printf("WARNING: No physics body - shape type is None\n");
			break;
		}
		}

		// CRITICAL: Register the entity-body mapping after creating the body
		if (rb.bodyID != 0) {
			Physics::PhysicsManager::RegisterEntityBody(entity, rb.bodyID);
			printf("Registered entity %d with body ID %d\n", entity, rb.bodyID);
		}
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
		// Check for collider changes and recreate physics bodies if needed
		CheckForColliderChanges();

		if (NE::GetEngineState() == EngineState::Play) {
			Physics::PhysicsManager::Update(static_cast<float>(dt));

			const auto& entities = GetEntities();
			for (Entity e : entities) {
				auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);
				auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

				Math::Vec3 pos;
				Math::Vec3 rot;
				Physics::PhysicsManager::GetTransform(rb.bodyID, pos, rot);

				transform.position = pos;
				transform.rotation = rot;
				transform.isDirty = true;
			}
		} 
		else {
			const auto& entities = GetEntities();
			for (Entity e : entities) {
				auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);
				auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

				if (transform.isDirty) {
					Physics::PhysicsManager::SetTransform(
						rb.bodyID, transform.position, transform.rotation);
				}

				if (rb.isStatic) {
					Physics::PhysicsManager::SetMotionType(rb.bodyID, JPH::EMotionType::Static);
				}
			}
		}
	}

	void RigidbodySystem::Exit()
	{
	}

}