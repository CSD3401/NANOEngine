#include "RigidbodySystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../../Physics/PhysicsManager.hpp"

namespace NANOEngine::ECS::Systems {

	RigidbodySystem::RigidbodySystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void RigidbodySystem::OnEntityAdded(Entity entity)
	{
		if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
			return;
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
		Component::Collider* col = nullptr;
		if (m_componentManager->HasComponent<Component::Collider>(entity))
			col = &m_componentManager->GetComponent<Component::Collider>(entity);
		//rb.bodyId = Physics::PhysicsManager::CreateBody(transform, col);
	}

	void RigidbodySystem::OnEntityRemoved(Entity entity)
	{
		if (!m_componentManager->HasComponent<Component::Rigidbody>(entity))
			return;
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
		Physics::PhysicsManager::DestroyBody(rb.bodyId);
	}

	void RigidbodySystem::Init()
	{
	}

	void RigidbodySystem::Update(double dt)
	{
		Physics::PhysicsManager::Update(static_cast<float>(dt));
		const auto& entities = GetEntities();
		for (Entity e : entities) {
			auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);
			auto& transform = m_componentManager->GetComponent<Component::Transform>(e);
			Math::Vec3 pos;
			Math::Vec3 rot;
			//Physics::PhysicsManager::GetTransform(rb.bodyId, pos, rot);
			transform.position = pos;
			transform.rotation = rot;
			transform.isDirty = true;
		}
	}

	void RigidbodySystem::Exit()
	{
	}

}