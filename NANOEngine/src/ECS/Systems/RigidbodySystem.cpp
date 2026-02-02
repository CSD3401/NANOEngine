#include "RigidbodySystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../Components/EntityMeta.hpp"
#include "Physics/PhysicsManager.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"
#include <Core/Profiler.hpp>

namespace NE::ECS::Systems {

	RigidbodySystem::RigidbodySystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr)
		: m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) { }

	void RigidbodySystem::OnEntityAdded(Entity e) {
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);

		if (rb.luid == 0)
			rb.luid = Core::LUIDGenerator::Generate("rb");

		m_luidRegistry->Register(rb.luid, &rb, e);
	}

	void RigidbodySystem::OnEntityRemoved(Entity e) {
		auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(e);
		m_luidRegistry->Unregister(rb.luid);
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

	void RigidbodySystem::Update(double /*dt*/) {
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif

		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			Physics::PhysicsManager::GetInstance().SyncTransformToBodies(meta.luid, t);
		}
	}

	void RigidbodySystem::Exit() {

	}

}