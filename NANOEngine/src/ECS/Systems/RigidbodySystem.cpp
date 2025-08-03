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

namespace NANOEngine::ECS::Systems {

	RigidbodySystem::RigidbodySystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void RigidbodySystem::OnEntityAdded(Entity entity)
	{
		//if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
		//	return;
		////auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		////auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
		//Component::Collider* col = nullptr;
		//if (m_componentManager->HasComponent<Component::Collider>(entity))
		//	col = &m_componentManager->GetComponent<Component::Collider>(entity);
		////rb.bodyId = Physics::PhysicsManager::CreateBody(transform, col);

		if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
			return;
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

		JPH::Quat rotation = JPH::Quat::sEulerAngles({ JPH::DegreesToRadians(transform.rotation.x),
														JPH::DegreesToRadians(transform.rotation.y),
														JPH::DegreesToRadians(transform.rotation.z) });
		//JPH::BodyCreationSettings settings(Physics::PhysicsManager::s_shapeMap.at(entity).GetPtr(),
		//	{ transform.position.x, transform.position.y, transform.position.z }, rotation,
		//	rb.motionType, rb.motionType == 0U ? 0 : 1);
		//settings.mLinearVelocity = { rb.initialVelocity.x, rb.initialVelocity.y, rb.initialVelocity.z };

		//auto shapePtr = Physics::PhysicsManager::s_shapeMap.at(entity).GetPtr();
		//auto it = Physics::PhysicsManager::s_shapeMap.find(entity);
		//if (it == Physics::PhysicsManager::s_shapeMap.end())
		//	return; // collider shape not found
		//auto shapePtr = it->second.GetPtr();
		JPH::RVec3 position(transform.position.x, transform.position.y, transform.position.z);
		//JPH::Quat rotation = ...; // already created

		// Map your POD uint8_t to Jolt EMotionType:
		JPH::EMotionType joltMotionType = JPH::EMotionType::Dynamic;
		//switch (rb.motionType) {
		//case 0: joltMotionType = JPH::EMotionType::Static; break;
		//case 1: joltMotionType = JPH::EMotionType::Kinematic; break;
		//case 2: joltMotionType = JPH::EMotionType::Dynamic; break;
		//}

		JPH::ObjectLayer objectLayer = (rb.motionType == 0U ? 0 : 1);

		//JPH::BodyCreationSettings settings(shapePtr, position, rotation, joltMotionType, objectLayer);
		//settings.mLinearVelocity = { rb.initialVelocity.x, rb.initialVelocity.y, rb.initialVelocity.z };

		//rb.bodyID = Physics::PhysicsManager::CreateBody(settings);
		//JPH::BodyID id = Physics::PhysicsManager::CreateBody(settings);

		JPH::BodyCreationSettings boxSettings(new JPH::BoxShape({ 0.5f, 0.5f, 0.5f }), position, rotation, joltMotionType, objectLayer);
		rb.bodyID = Physics::PhysicsManager::CreateBody(boxSettings);

		printf("RigidbodySystem: Created body with ID %d for entity %d\n", rb.bodyID, entity);
	}

	void RigidbodySystem::OnEntityRemoved(Entity entity)
	{
		if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
			return;
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		Physics::PhysicsManager::DestroyBody(rb.bodyID);
	}

	void RigidbodySystem::Init()
	{
	}

	void RigidbodySystem::Update(double dt)
	{
		if (NANOEngine::GetEngineState() == EngineState::Play) {
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
			}
		}
	}

	void RigidbodySystem::Exit()
	{
	}

}