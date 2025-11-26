#include "TransformSystem.hpp"
#include "../Components/Transform.hpp"
#include "../Components/EntityMeta.hpp"
#include "PrefabManagement/PrefabManager.hpp"
#include "../../Core/Profiler.hpp"
#include "../../src/EngineState.hpp"
#include <vector>                      

namespace {


	static NE::Math::Mat4 InverseTRS(const NE::Math::Mat4& world)
	{
		using namespace NE::Math;
		const Vec3 p = world.GetTranslation();
		const Vec3 r = world.GetRotation(); // Euler (same order you use to build)
		const Vec3 s = world.GetScale();

		// (T*R*S)^-1 = S^-1 * R^-1 * T^-1
		Mat4 invT = Mat4::BuildTranslation(Vec3{ -p.x, -p.y, -p.z });
		Mat4 invR = Mat4::BuildZRotation(-r.z) * Mat4::BuildYRotation(-r.y) * Mat4::BuildXRotation(-r.x);
		Mat4 invS = Mat4::BuildScaling(1.0f / s.x, 1.0f / s.y, 1.0f / s.z);
		return invS * invR * invT;
	}

	static void DecomposeToTRS(const NE::Math::Mat4& m,
		NE::Math::Vec3& outPos,
		NE::Math::Vec3& outRot,
		NE::Math::Vec3& outScale)
	{
		outPos = m.GetTranslation();
		outRot = m.GetRotation();
		outScale = m.GetScale();
	}

}

namespace NE::ECS::Systems {

	TransformSystem::TransformSystem(ComponentManager* cm) : m_componentManager(cm) {
	}

	void TransformSystem::OnEntityAdded(Entity e) {
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);

		if (t.luid != 0) {
			m_luidToEntity[t.luid] = e;
		}

		if (t.parentLuid != 0) {
			m_pendingParents.push_back({ e, t.parentLuid });
		}

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
	}

	void TransformSystem::OnEntityRemoved(Entity) {

	}

	void TransformSystem::Init() {
		ResolvePendingParentsForAll(/*keepWorldForNewParents=*/false);

		const auto& entities = GetEntities();

		BuildLocalMatrices();

		Math::Mat4 I;
		I.SetToIdentity();

		for (Entity e : entities) {
			auto& tr = m_componentManager->GetComponent<Component::Transform>(e);

			if (tr.parent == Component::INVALID_PARENT) {
				UpdateWorldRecursive(e, I);
			}
		}

		//for (Entity e : entities) {
		//	auto& meta = m_componentManager->GetComponent<Component::EntityMeta>(e);
		//	if (!meta.prefabID.empty()) {
		//		
		//	}
		//}
	}

	void TransformSystem::Update(double) {
		NE_PROFILE_FUNCTION();

		const auto& entities = GetEntities();

		BuildLocalMatrices();

		Math::Mat4 I;
		I.SetToIdentity();

		for (Entity e : entities) {
			auto& tr = m_componentManager->GetComponent<Component::Transform>(e);

			if (tr.parent == Component::INVALID_PARENT) {
				UpdateWorldRecursive(e, I);
			}
		}
	}

	void TransformSystem::Exit() {
	}
	
	void TransformSystem::SetParent(Entity child, Entity newParent, bool keepWorld) {
		auto& childT = m_componentManager->GetComponent<Component::Transform>(child);

		NE::Math::Mat4 childWorldBefore;
		if (keepWorld) {
			childWorldBefore = childT.worldMatrix;
		}

		if (childT.parent != Component::INVALID_PARENT) {
			auto& oldParentT = m_componentManager->GetComponent<Component::Transform>(childT.parent);
			auto& vec = oldParentT.children;
			vec.erase(std::remove(vec.begin(), vec.end(), child), vec.end());
		}

		childT.parent = newParent;

		if (newParent != Component::INVALID_PARENT) {
			auto& parentT = m_componentManager->GetComponent<Component::Transform>(newParent);
			parentT.children.push_back(child);
			childT.parentLuid = parentT.luid;
		} else {
			childT.parentLuid = 0;
		}

		if (!keepWorld) {
			MarkDirtyRecursive(child);
			return;
		}

		NE::Math::Mat4 localM;
		if (newParent != Component::INVALID_PARENT) {
			auto& parentT = m_componentManager->GetComponent<Component::Transform>(newParent);
			NE::Math::Mat4 invParent = InverseTRS(parentT.worldMatrix);
			localM = invParent * childWorldBefore;
		} else {
			localM = childWorldBefore;
		}

		DecomposeToTRS(localM, childT.localPosition, childT.localRotationEuler, childT.localScale);
		MarkDirtyRecursive(child);
	}

	void TransformSystem::MarkDirtyRecursive(Entity e) {
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);
		if (t.isDirty) return;
		t.isDirty = true;
		for (Entity child : t.children) {
			MarkDirtyRecursive(child);
		}
	}

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

	void TransformSystem::UpdateWorldRecursive(Entity e, const Math::Mat4& parentWorld) {
		auto& t = m_componentManager->GetComponent<Component::Transform>(e);

		t.worldMatrix = parentWorld * t.localMatrix;
		t.isDirty = false;

		for (Entity child : t.children) {
			UpdateWorldRecursive(child, t.worldMatrix);
		}
	}

	void TransformSystem::ResolvePendingParentsForAll(bool keepWorldForNewParents) {
		std::vector<PendingParent> stillPending;
		stillPending.reserve(m_pendingParents.size());

		for (const PendingParent& pp : m_pendingParents) {
			if (!m_componentManager->HasComponent<Component::Transform>(pp.child))
				continue;

			auto& childT = m_componentManager->GetComponent<Component::Transform>(pp.child);

			auto it = m_luidToEntity.find(pp.parentLuid);
			if (it != m_luidToEntity.end()) {
				Entity parentEnt = it->second;

				// For prefab in the future
				SetParent(pp.child, parentEnt, keepWorldForNewParents);
			} else {
				stillPending.push_back(pp);
			}
		}

		m_pendingParents.swap(stillPending);
	}

}
