#include "TransformSystem.hpp"
#include "../Components/Transform.hpp"
#include "../../Core/Profiler.hpp"
#include "../../src/EngineState.hpp"

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

	void TransformSystem::Update(double)
	{
		//NE_PROFILE_FUNCTION();
		//const auto& entities = GetEntities();
		//for (Entity e : entities) {
		//	auto& transform = m_componentManager->GetComponent<Component::Transform>(e);
		//	if (transform.isDirty) {
		//		Math::Mat4 translation = Math::Mat4::BuildTranslation(transform.position);
		//		Math::Mat4 rotation = Math::Mat4::BuildXRotation(transform.rotation.x) *
		//			Math::Mat4::BuildYRotation(transform.rotation.y) *
		//			Math::Mat4::BuildZRotation(transform.rotation.z);
		//		Math::Mat4 scale = Math::Mat4::BuildScaling(transform.scale.x, transform.scale.y, transform.scale.z);

		//		transform.modelMatrix = translation * rotation * scale;
		//		transform.isDirty = false;
		//	}
		//}


		//NE_PROFILE_FUNCTION();

		//const auto& entities = GetEntities();

		//for (Entity e : entities) {
		//	auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

		//	// --- Ownership Control ---
		//	if (NANOEngine::GetEngineState() == EngineState::Play) {
		//		// In play mode, RigidbodySystem is authoritative
		//		// Do NOT rebuild modelMatrix here (physics will sync it).
		//		continue;
		//	}

		//	// In Edit or Paused mode, editor / gizmos are authoritative
		//	if (transform.isDirty) {
		//		Math::Mat4 translation = Math::Mat4::BuildTranslation(transform.position);

		//		Math::Mat4 rotation =
		//			Math::Mat4::BuildXRotation(transform.rotation.x) *
		//			Math::Mat4::BuildYRotation(transform.rotation.y) *
		//			Math::Mat4::BuildZRotation(transform.rotation.z);

		//		Math::Mat4 scale =
		//			Math::Mat4::BuildScaling(transform.scale.x, transform.scale.y, transform.scale.z);

		//		transform.modelMatrix = translation * rotation * scale;
		//		transform.isDirty = false;
		//	}
		//}

		NE_PROFILE_FUNCTION();
		const auto& entities = GetEntities();

		for (Entity e : entities) {
			auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

			bool shouldRebuild = false;

			if (NANOEngine::GetEngineState() == EngineState::Play) {
				// Physics updated position/rotation already in RigidbodySystem
				// Always rebuild matrix in Play mode
				shouldRebuild = true;
			} else {
				// Edit or Paused mode, rebuild only if dirty (gizmo changed)
				if (transform.isDirty)
					shouldRebuild = true;
			}

			if (shouldRebuild) {
				Math::Mat4 translation = Math::Mat4::BuildTranslation(transform.position);

				Math::Mat4 rotation =
					Math::Mat4::BuildXRotation(transform.rotation.x) *
					Math::Mat4::BuildYRotation(transform.rotation.y) *
					Math::Mat4::BuildZRotation(transform.rotation.z);

				Math::Mat4 scale =
					Math::Mat4::BuildScaling(transform.scale.x, transform.scale.y, transform.scale.z);

				transform.modelMatrix = translation * rotation * scale;
				transform.isDirty = false;
			}
		}
	}

	void TransformSystem::Exit()
	{
	}

}

