#include "CharacterControllerSystem.hpp"

#include "ECS/Core/EntityManager.hpp"
#include "ECS/Core/ComponentManager.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Physics/PhysicsManager.hpp"

#include "../Components/EntityMeta.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Collider.hpp"
#include "ECS/Components/CharacterController.hpp"

namespace NE::ECS::Systems {
	CharacterControllerSystem::CharacterControllerSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr)
		: m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) { }

	void CharacterControllerSystem::OnEntityAdded(Entity e) {
		auto& cc = m_componentManager->GetComponent<Component::CharacterController>(e);

		if (cc.luid == 0)
			cc.luid = Core::LUIDGenerator::Generate("cc");

		m_luidRegistry->Register(cc.luid, &cc, e);
	}

	void CharacterControllerSystem::OnEntityRemoved(Entity e) {
		auto& cc = m_componentManager->GetComponent<Component::CharacterController>(e);
		m_luidRegistry->Unregister(cc.luid);
	}

	void CharacterControllerSystem::Init() {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			auto& rb = m_componentManager->GetComponent<Component::CharacterController>(e);
			//auto& col = m_componentManager->GetComponent<Component::Collider>(e);

			Physics::PhysicsManager::GetInstance().CreateCharacterController(
				e, meta.luid, 
				t, rb,
				static_cast<uint8_t>(m_entityManager->GetLayer(e))
			);
		}
	}

	void CharacterControllerSystem::Update(double /*deltaTime*/) {
		auto& allEntities = m_entities.GetDenseContainer();

		for (auto& e : allEntities) {
			auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
			auto& t = m_componentManager->GetComponent<Component::Transform>(e);
			Physics::PhysicsManager::GetInstance().SyncTransformToCharacters(meta.luid, t);
		}
	}

	void CharacterControllerSystem::Exit() {

	}

}
