#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Collider.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace NE::ECS::Systems {

	class RigidbodySystem final : public System {
	public:
		explicit RigidbodySystem(ComponentManager* cm);

		void OnEntityAdded(Entity entity) override;
		void OnEntityRemoved(Entity entity) override;

		// Recreates physics body
		//void OnShapeChange(Entity entity, 
		//	Component::Collider::ShapeType oldShape,
		//	Component::Collider::ShapeType newShape);

		//void OnPropertyChange(Entity entity);

		void CreatePhysicsBodyFromComponent(Entity entity,
			Component::Transform& transform,
			Component::Rigidbody& rb,
			Component::Collider& collider,
			JPH::EMotionType motionType);

		void CheckForColliderChanges();
		void RecreatePhysicsBody(Entity entity);

		void Init() override;
		void Update(double deltaTime) override;
		void Exit() override;

	private:
		ComponentManager* m_componentManager;
	};

}