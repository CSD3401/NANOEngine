#include "TransformSystem.hpp"
#include "../Components/Transform.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/Hierarchy.hpp"
#include "Core/Profiler.hpp"
#include "Core/LUIDGenerator.hpp"

namespace NE::ECS::Systems {

	TransformSystem::TransformSystem(ComponentManager* cm) : m_componentManager(cm) { }

	void TransformSystem::OnEntityAdded(Entity e) {
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);

		Math::Mat4 translation = Math::Mat4::BuildTranslation(t.localPosition);

		Math::Mat4 rotation =
			Math::Mat4::BuildXRotation(t.localRotationEuler.x) *
			Math::Mat4::BuildYRotation(t.localRotationEuler.y) *
			Math::Mat4::BuildZRotation(t.localRotationEuler.z);

		Math::Mat4 scale =
			Math::Mat4::BuildScaling(t.localScale.x,
				t.localScale.y,
				t.localScale.z);

		t.localMatrix = translation * rotation * scale;
		t.worldMatrix = t.localMatrix;

		if (t.luid == 0)
			t.luid = Core::LUIDGenerator::Generate("tr");
	}

	void TransformSystem::OnEntityRemoved(Entity) {
	}

	void TransformSystem::Init() {
		//const auto& entities = GetEntities();

		//BuildLocalMatrices();

		//Math::Mat4 I;
		//I.SetToIdentity();

		//for (Entity e : entities) {
		//	auto& h = m_componentManager->GetComponent<Component::Hierarchy>(e);
		//	if (h.parent == Component::INVALID_PARENT) {
		//		UpdateWorldRecursive(e, I, false);
		//	}
		//}
		Update(0.0);
	}

	void TransformSystem::Update(double) {
		NE_PROFILE_FUNCTION();

		const auto& entities = GetEntities();

		BuildLocalMatrices();

		Math::Mat4 I;
		I.SetToIdentity();

		for (Entity e : entities) {
			auto& h = m_componentManager->GetComponent<Component::Hierarchy>(e);
			if (h.parent == Component::INVALID_PARENT) {
				UpdateWorldRecursive(e, I, false);
			}
		}
	}

	void TransformSystem::Exit() { }

	void TransformSystem::BuildLocalMatrices() {
		const auto& entities = GetEntities();

		for (Entity e : entities) {
			auto& transform = m_componentManager->GetComponent<Component::Transform>(e);

			if (!transform.isDirty)
				continue;

			Math::Mat4 translation = Math::Mat4::BuildTranslation(transform.localPosition);

			Math::Mat4 rotation =
				Math::Mat4::BuildXRotation(transform.localRotationEuler.x) *
				Math::Mat4::BuildYRotation(transform.localRotationEuler.y) *
				Math::Mat4::BuildZRotation(transform.localRotationEuler.z);

			Math::Mat4 scale =
				Math::Mat4::BuildScaling(transform.localScale.x,
					transform.localScale.y,
					transform.localScale.z);

			transform.localMatrix = translation * rotation * scale;
		}
	}

	void TransformSystem::UpdateWorldRecursive(Entity e,
		const Math::Mat4& parentWorld,
		bool parentWorldDirty)
	{
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);
		auto& h = m_componentManager->GetComponent<Component::Hierarchy>(e);

		bool localDirty = t.isDirty;
		bool worldDirty = parentWorldDirty || localDirty;

		if (worldDirty) {
			t.worldMatrix = parentWorld * t.localMatrix;
			t.isDirty = false;
		}

		for (uint32_t child : h.children) {
			UpdateWorldRecursive(child, t.worldMatrix, worldDirty);
		}
	}

}