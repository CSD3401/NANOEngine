#include "PhysicsExports.hpp"
#include "../SceneManagement/Scene.hpp"
#include "../ECS/Components/Transform.hpp"
#include "../ECS/Components/Collider.hpp"
#include "../Physics/PhysicsManager.hpp"



namespace NE {
	SceneManagement::Scene& GetScene();
}

namespace NE::Physics 
{

	namespace Query 
	{
		bool HasPhysicsBody(uint32_t entity)
		{
			return Physics::PhysicsManager::EntityHasPhysicsBody(entity);
		}

		uint32_t GetPhysicsBodyId(uint32_t entity)
		{
			return Physics::PhysicsManager::GetEntityBodyId(entity);
		}

		void GetPhysicsTransform(uint32_t entity, Math::Vec3& position, Math::Vec3& rotation)
		{
			uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(entity);
			if (bodyID != 0)
			{
				Physics::PhysicsManager::GetTransform(bodyID, position, rotation);
			}
		}

		void GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation)
		{
			Physics::PhysicsManager::GetTransform(index, position, rotation);
		}

	}

	namespace Command 
	{
		void Init()
		{
			Physics::PhysicsManager::Init();
		}

		void Update(float dt)
		{
			Physics::PhysicsManager::Update(dt);
		}

		void Shutdown()
		{
			Physics::PhysicsManager::Shutdown();
		}

		void ActivateBodies()
		{
			Physics::PhysicsManager::ActivateBodies();
		}

		void DeactivateBodies()
		{
			Physics::PhysicsManager::DeactivateBodies();
		}

		void DestroyBody(uint32_t index)
		{
			Physics::PhysicsManager::DestroyBody(index);
		}

		void RegisterEntityBody(uint32_t entity, uint32_t bodyID)
		{
			Physics::PhysicsManager::RegisterEntityBody(entity, bodyID);
		}

		void UnregisterEntityBody(uint32_t entity)
		{
			Physics::PhysicsManager::UnregisterEntityBody(entity);
		}


		void CreatePhysicsBody(uint32_t entity)
		{
			// return if alr exists
			if (Physics::PhysicsManager::GetEntityBodyId(entity) != 0)
				return;

			auto& transform = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Transform>(entity);
			auto &collider = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(entity);

			if (collider.shapeType == NE::ECS::Component::Collider::ShapeType::Box)
			{
				Math::Vec3 fullSize = {
					collider.halfExtents.x * 2.0f,
					collider.halfExtents.y * 2.0f,
					collider.halfExtents.z * 2.0f
				};

				uint32_t bodyId = Physics::PhysicsManager::CreateBoxBody(
					transform.position,
					transform.rotation,
					fullSize,
					JPH::EMotionType::Dynamic
				);

				if (bodyId != 0)
					Physics::PhysicsManager::RegisterEntityBody(entity, bodyId);

			}


		}

		void UpdatePhysicsBody(uint32_t entity)
		{
			uint32_t bodyId = Physics::PhysicsManager::GetEntityBodyId(entity);
			if (bodyId == 0)
				return;

			auto& collider = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(entity);

			if (collider.shapeType == NE::ECS::Component::Collider::ShapeType::Box)
			{
				Math::Vec3 fullSize = {
					collider.halfExtents.x * 2.0f,
					collider.halfExtents.y * 2.0f,
					collider.halfExtents.z * 2.0f
				};


				Physics::PhysicsManager::UpdateBoxSize(bodyId, fullSize);

			}
		}

		void RemovePhysicsBody(uint32_t entity) 
		{
			uint32_t bodyId = Physics::PhysicsManager::GetEntityBodyId(entity);
			if (bodyId != 0) 
			{
				Physics::PhysicsManager::DestroyBody(bodyId);
				Physics::PhysicsManager::UnregisterEntityBody(entity);
			}
		}

		void SetPhysicsMotionType(uint32_t entity, int motionType) 
		{
			uint32_t bodyId = Physics::PhysicsManager::GetEntityBodyId(entity);
			if (bodyId != 0) 
			{
				Physics::PhysicsManager::SetMotionType(bodyId, static_cast<JPH::EMotionType>(motionType));
			}
		}
	}


}
