#include "RigidbodySystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../Components/EntityMeta.hpp"
#include "Physics/PhysicsManager.hpp"


namespace NE::ECS::Systems {

	RigidbodySystem::RigidbodySystem(ComponentManager* cm, EntityManager* em) 
		: m_componentManager(cm), m_entityManager(em)
	{
	}

	void RigidbodySystem::OnEntityAdded(Entity entity) {

	}

	void RigidbodySystem::OnEntityRemoved(Entity entity) {

	}

	void RigidbodySystem::Init() {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);
			auto& col = m_componentManager->GetComponent<Component::Collider>(e);

			Physics::PhysicsManager::GetInstance().CreateBody(e, meta.luid, t, rb, col, static_cast<uint8_t>(m_entityManager->GetLayer(e)));
		}
	}

	void RigidbodySystem::Update(double dt) {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			Physics::PhysicsManager::GetInstance().SyncBodiesToTransform(meta.luid, t);
		}
	}

	void RigidbodySystem::Exit() {

	}

}