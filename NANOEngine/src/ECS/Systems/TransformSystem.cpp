#include "TransformSystem.hpp"
#include "../Components/Transform.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/Hierarchy.hpp"
#include "Core/Profiler.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Math/Quat.hpp"

namespace NE::ECS::Systems {

	TransformSystem::TransformSystem(ComponentManager* cm, Core::LUIDRegistry* lr) : m_componentManager(cm), m_luidRegistry(lr) { }

	void TransformSystem::OnEntityAdded(Entity e) {
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);

		Math::Mat4 translation = Math::Mat4::BuildTranslation(t.localPosition);

		// Convert Euler angles to quaternion, then to rotation matrix
		Math::Mat4 rotation = Math::Quat::FromEulerDegrees(t.localRotationEuler).ToMat4();

		Math::Mat4 scale =
			Math::Mat4::BuildScaling(t.localScale.x,
				t.localScale.y,
				t.localScale.z);

		t.localMatrix = translation * rotation * scale;
		t.worldMatrix = t.localMatrix;

		if (t.luid == 0)
			t.luid = Core::LUIDGenerator::Generate("tr");

		m_luidRegistry->Register(t.luid, &t, e);
	}

	void TransformSystem::OnEntityRemoved(Entity e) {
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);
		m_luidRegistry->Unregister(t.luid);
	}

	void TransformSystem::Init() {
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

			// Convert Euler angles to quaternion, then to rotation matrix
			Math::Mat4 rotation = Math::Quat::FromEulerDegrees(transform.localRotationEuler).ToMat4();

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