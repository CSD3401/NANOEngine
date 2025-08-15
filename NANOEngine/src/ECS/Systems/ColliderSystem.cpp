#include "ColliderSystem.hpp"
#include "../Components/Rigidbody.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Collider.hpp"
#include "../../Physics/PhysicsManager.hpp"
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>


namespace NE::ECS::Systems {

	ColliderSystem::ColliderSystem(ComponentManager* cm) : m_componentManager(cm)
	{
	}

	void ColliderSystem::OnEntityAdded(Entity)
	{
        //auto& col = m_componentManager->GetComponent<Component::Collider>(entity);

        //using ShapeType = Component::Collider::ShapeType;
        //JPH::RefConst<JPH::Shape> shape;

        //switch (col.shapeType) {
        //case ShapeType::Box:
        //{
        //    JPH::BoxShapeSettings settings({ col.halfExtents.x, col.halfExtents.y, col.halfExtents.z });
        //    auto result = settings.Create();
        //    if (!result.HasError())
        //        shape = result.Get();
        //    break;
        //}
        //case ShapeType::Sphere:
        //{
        //    JPH::SphereShapeSettings settings(col.radius);
        //    auto result = settings.Create();
        //    if (!result.HasError())
        //        shape = result.Get();
        //    break;
        //}
        //case ShapeType::Capsule:
        //{
        //    JPH::CapsuleShapeSettings settings(col.height * 0.5f, col.radius);
        //    auto result = settings.Create();
        //    if (!result.HasError())
        //        shape = result.Get();
        //    break;
        //}
        //default:
        //{
        //    JPH::BoxShapeSettings settings({ col.halfExtents.x, col.halfExtents.y, col.halfExtents.z });
        //    auto result = settings.Create();
        //    if (!result.HasError())
        //        shape = result.Get();
        //    break;
        //}
        //}

        //Physics::PhysicsManager::s_shapeMap[entity] = shape;
        //col.shape = shape;
	}

	void ColliderSystem::OnEntityRemoved(Entity entity)
	{
        //if (!m_componentManager->HasComponent<Component::Collider>(entity))
        //    return;
        //auto& col = m_componentManager->GetComponent<Component::Collider>(entity);
		Physics::PhysicsManager::s_shapeMap.erase(entity);
        //col.shape = nullptr;
	}

	void ColliderSystem::Init()
	{
	}

	void ColliderSystem::Update(double)
	{

	}

	void ColliderSystem::Exit()
	{
	}

}