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
		//bool HasPhysicsBody(uint32_t entity)
		//{
		//	return Physics::PhysicsManager::EntityHasPhysicsBody(entity);
		//}

		//uint32_t GetPhysicsBodyId(uint32_t entity)
		//{
		//	return Physics::PhysicsManager::GetEntityBodyId(entity);
		//}

		//void GetPhysicsTransform(uint32_t entity, Math::Vec3& position, Math::Vec3& rotation)
		//{
		//	uint32_t bodyID = Physics::PhysicsManager::GetEntityBodyId(entity);
		//	if (bodyID != 0)
		//	{
		//		Physics::PhysicsManager::GetTransform(bodyID, position, rotation);
		//	}
		//}

		//void GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation)
		//{
		//	Physics::PhysicsManager::GetTransform(index, position, rotation);
		//}

	}

	namespace Command 
	{
		//void Init()
		//{
		//	Physics::PhysicsManager::Init();
		//}

		//void Update(float dt)
		//{
		//	Physics::PhysicsManager::Update(dt);
		//}

		//void Shutdown()
		//{
		//	Physics::PhysicsManager::Shutdown();
		//}

		//void ActivateBodies()
		//{
		//	Physics::PhysicsManager::ActivateBodies();
		//}

		//void DeactivateBodies()
		//{
		//	Physics::PhysicsManager::DeactivateBodies();
		//}

		//void DestroyBody(uint32_t index)
		//{
		//	Physics::PhysicsManager::DestroyBody(index);
		//}

		//void RegisterEntityBody(uint32_t entity, uint32_t bodyID)
		//{
		//	Physics::PhysicsManager::RegisterEntityBody(entity, bodyID);
		//}

		//void UnregisterEntityBody(uint32_t entity)
		//{
		//	Physics::PhysicsManager::UnregisterEntityBody(entity);
		//}


		//void CreatePhysicsBody(uint32_t entity)
		//{
		//	// return if alr exists
		//	if (PhysicsManager::EntityHasPhysicsBody(entity))
		//		return;

		//	auto& transform = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Transform>(entity);
		//	auto &collider = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(entity);

		//	if (collider.shapeType == NE::ECS::Component::Collider::ShapeType::Box)
		//	{
		//		Math::Vec3 fullSize = {
		//			collider.halfExtents.x * 2.0f,
		//			collider.halfExtents.y * 2.0f,
		//			collider.halfExtents.z * 2.0f
		//		};

		//		uint32_t bodyId = Physics::PhysicsManager::CreateBoxBody(
		//			transform.position,
		//			transform.rotation,
		//			fullSize,
		//			JPH::EMotionType::Dynamic
		//		);

		//		if (bodyId != 0)
		//			Physics::PhysicsManager::RegisterEntityBody(entity, bodyId);
		//	}
		//	else
		//	{
		//		printf("unsupported shape time %d\n", static_cast<int>(collider.shapeType));
		//	}
		//}

		//void CreatePhysicsBody(uint32_t entity)
		//{
		//	printf("=== CreatePhysicsBody START for entity %u ===\n", entity);

		//	// return if alr exists
		//	if (PhysicsManager::EntityHasPhysicsBody(entity)) {
		//		printf("Entity %u already has physics body, skipping\n", entity);
		//		return;
		//	}

		//	auto& coordinator = NE::GetScene().GetECSCoordinator();

		//	// Check if components exist
		//	if (!coordinator.HasComponent<NE::ECS::Component::Transform>(entity)) {
		//		printf("ERROR: Entity %u has no Transform component!\n", entity);
		//		return;
		//	}
		//	if (!coordinator.HasComponent<NE::ECS::Component::Collider>(entity)) {
		//		printf("ERROR: Entity %u has no Collider component!\n", entity);
		//		return;
		//	}

		//	auto& transform = coordinator.GetComponent<NE::ECS::Component::Transform>(entity);
		//	auto& collider = coordinator.GetComponent<NE::ECS::Component::Collider>(entity);

		//	printf("Transform: pos(%.2f, %.2f, %.2f) rot(%.2f, %.2f, %.2f)\n",
		//		transform.localPosition.x, transform.localPosition.y, transform.localPosition.z,
		//		transform.localRotationEuler.x, transform.localRotationEuler.y, transform.localRotationEuler.z);
		//	printf("Collider shapeType: %d\n", static_cast<int>(collider.shapeType));

		//	if (collider.shapeType == NE::ECS::Component::Collider::ShapeType::Box)
		//	{
		//		Math::Vec3 fullSize = {
		//			collider.halfExtents.x * 2.0f,
		//			collider.halfExtents.y * 2.0f,
		//			collider.halfExtents.z * 2.0f
		//		};

		//		printf("Box collider - halfExtents: (%.2f, %.2f, %.2f)\n",
		//			collider.halfExtents.x, collider.halfExtents.y, collider.halfExtents.z);
		//		printf("Box collider - fullSize: (%.2f, %.2f, %.2f)\n",
		//			fullSize.x, fullSize.y, fullSize.z);

		//		uint32_t bodyId = Physics::PhysicsManager::CreateBoxBody(
		//			transform.localPosition,
		//			transform.localRotationEuler,
		//			fullSize,
		//			JPH::EMotionType::Dynamic, entity, meta.layer
		//		);

		//		printf("CreateBoxBody returned: %u\n", bodyId);

		//		if (bodyId != 0) {
		//			Physics::PhysicsManager::RegisterEntityBody(entity, bodyId);
		//			printf("SUCCESS: Registered physics body %u for entity %u\n", bodyId, entity);
		//		}
		//		else {
		//			printf("FAILED: CreateBoxBody returned 0 - body creation failed!\n");
		//		}
		//	}
		//	else
		//	{
		//		printf("Unsupported shape type: %d\n", static_cast<int>(collider.shapeType));
		//	}

		//	printf("=== CreatePhysicsBody END for entity %u ===\n", entity);
		//}

		//void UpdatePhysicsBody(uint32_t entity)
		//{
		//	uint32_t bodyId = Physics::PhysicsManager::GetEntityBodyId(entity);
		//	if (bodyId == 0)
		//		return;

		//	//auto& collider = NE::GetScene().GetECSCoordinator().GetComponent<NE::ECS::Component::Collider>(entity);

		//	//if (collider.shapeType == NE::ECS::Component::Collider::ShapeType::Box)
		//	//{
		//	//	Math::Vec3 fullSize = {
		//	//		collider.halfExtents.x * 2.0f,
		//	//		collider.halfExtents.y * 2.0f,
		//	//		collider.halfExtents.z * 2.0f
		//	//	};


		//	//	Physics::PhysicsManager::UpdateBoxSize(bodyId, fullSize);
		//	//}
		//}

		//void RemovePhysicsBody(uint32_t entity) 
		//{
		//	uint32_t bodyId = Physics::PhysicsManager::GetEntityBodyId(entity);
		//	if (bodyId != 0) 
		//	{
		//		Physics::PhysicsManager::DestroyBody(bodyId);
		//		Physics::PhysicsManager::UnregisterEntityBody(entity);
		//	}
		//}

		//void SetPhysicsMotionType(uint32_t entity, int motionType) 
		//{
		//	uint32_t bodyId = Physics::PhysicsManager::GetEntityBodyId(entity);
		//	if (bodyId != 0) 
		//	{
		//		Physics::PhysicsManager::SetMotionType(bodyId, static_cast<JPH::EMotionType>(motionType));
		//	}
		//}

		//void LockConstraints(uint32_t entity, bool lockX, bool lockY, bool lockZ) {
		//	uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
		//	NE::Physics::PhysicsManager::LockRotation(bodyID, lockX, lockY, lockZ);
		//}
	}


}
