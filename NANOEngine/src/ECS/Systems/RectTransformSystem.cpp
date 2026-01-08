#include "RectTransformSystem.hpp"

#include "../Components/RectTransform.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/Hierarchy.hpp"
#include "Core/Profiler.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"

namespace NE::ECS::Systems {

	RectTransformSystem::RectTransformSystem(ComponentManager* cm, Core::LUIDRegistry* lr) 
		: m_componentManager(cm), m_luidRegistry(lr) {}

	void RectTransformSystem::OnEntityAdded(Entity e) {
		auto& rt = m_componentManager->GetComponent<Component::RectTransform>(e);

		if (rt.luid == 0)
			rt.luid = Core::LUIDGenerator::Generate("rt");

		m_luidRegistry->Register(rt.luid, &rt, e);
	}

	void RectTransformSystem::OnEntityRemoved(Entity e) {
		auto& rt = m_componentManager->GetComponent<Component::RectTransform>(e);
		m_luidRegistry->Unregister(rt.luid);
	}

	void RectTransformSystem::Init() {
		Update(0.0);
	}

	void RectTransformSystem::Update(double) {
		NE_PROFILE_FUNCTION();

		const auto& entities = GetEntities();

	}

	void RectTransformSystem::Exit() {}
}
