#include "TransformSystem.hpp"
#include "../Components/Transform.hpp"
#include "../../Core/Profiler.hpp"

namespace NANOEngine::ECS::Systems {

	TransformSystem::TransformSystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void TransformSystem::OnEntityAdded(Entity)
	{
	}

	void TransformSystem::OnEntityRemoved(Entity)
	{
		// TODO: remove parenting and stuff
	}

	void TransformSystem::Init()
	{
	}

	void TransformSystem::Update(double dt)
	{
		NE_PROFILE_FUNCTION();
		const auto& entities = GetEntities();
		for (Entity e : entities) {
			auto& transform = m_componentManager->GetComponent<Component::Transform>(e);
			if (transform.isDirty) {
				
				transform.isDirty = false;
			}

			//testing
			transform.rotation.y += static_cast<float>(dt * 100.0);
			transform.modelMatrix = Math::Mat4::BuildTranslation(transform.position) * Math::Mat4::BuildRotation(transform.rotation.y, {0.f, 1.f, 0.f});
		}
	}

	void TransformSystem::Exit()
	{
	}

}

