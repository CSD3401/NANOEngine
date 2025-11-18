#include "TransformSystem.hpp"
#include "../Components/Transform.hpp"
#include "../../Core/Profiler.hpp"
#include "../../src/EngineState.hpp"
#include "../Components/Parent.hpp"
#include <iostream>
#include <unordered_map>               
#include <vector>                      
#include <functional>


namespace NE::ECS::Systems {

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
		//test++;
		//std::cout << "test " << test << std::endl;
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

		const bool alwaysRebuild = (NE::GetEngineState() == EngineState::Play);

		using NE::Math::Mat4;

		const auto& entities = GetEntities();

		// Build adjacency: parent -> [children...]
		std::unordered_map<Entity, std::vector<Entity>> children;
		children.reserve(entities.size());

		for (Entity e : entities) {
			if (m_componentManager->HasComponent<Component::Parent>(e)) {
				Entity p = m_componentManager->GetComponent<Component::Parent>(e).parent;
				if (p != NO_ENTITY) children[p].push_back(e);
			}
		}

		// Identity for roots
		Mat4 I; I.SetToIdentity();

		std::function<void(Entity, Mat4, bool)> compute;

		compute = [&](Entity e, Mat4 parentWorld, bool parentNeedsUpdate)
			{
				auto& tr = m_componentManager->GetComponent<Component::Transform>(e);

				const bool needs = alwaysRebuild || tr.isDirty || parentNeedsUpdate;

				if (needs) {
					Mat4 T = Mat4::BuildTranslation(tr.position);
					Mat4 R = Mat4::BuildXRotation(tr.rotation.x)
						* Mat4::BuildYRotation(tr.rotation.y)
						* Mat4::BuildZRotation(tr.rotation.z);
					Mat4 S = Mat4::BuildScaling(tr.scale.x, tr.scale.y, tr.scale.z);

					Mat4 TRS = T * R * S;
					tr.modelMatrix = parentWorld * TRS;   // LHS is non-const Mat4
					tr.parent = parentWorld;
					tr.isDirty = false;
				}

				Mat4 world = tr.modelMatrix;              // make a (cheap) copy for recursion

				if (auto it = children.find(e); it != children.end()) {
					for (Entity c : it->second) {
						compute(c, world, needs);         // pass by value
					}
				}
			};

		// Start from roots (no Parent component OR Parent == NO_ENTITY)
		for (Entity e : entities) {
			bool hasParent = m_componentManager->HasComponent<Component::Parent>(e);
			Entity p = hasParent ? m_componentManager->GetComponent<Component::Parent>(e).parent : NO_ENTITY;
			if (p == NO_ENTITY) {
				compute(e, I, false);
			}
		}
	}

	void TransformSystem::Exit()
	{
	}

}

