#include "PhysicsManager.hpp"
#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterMask.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterMask.h>

// Raycasting includes
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Geometry/IndexedTriangle.h>
// Constraint includes for rotation locking
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>

// debug
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include "Core/SpdLogger.hpp"
#include "LayerMaskBodyFilter.hpp"

#include "ECS/Components/Collider.hpp"

namespace NE::Physics {
    PhysicsManager& PhysicsManager::GetInstance() {
        static PhysicsManager instance;
        return instance;
    }

    void PhysicsManager::Init() {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = &m_factory;
        JPH::RegisterTypes();

        m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        m_jobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
    }

    void PhysicsManager::CreateOrUpdateShape(const uint64_t& entityLUID, const ECS::Component::Collider& col) {
        using Collider = ECS::Component::Collider;

        JPH::ShapeRefC base;

        auto CreateShape = [&](auto&& settings) -> JPH::ShapeRefC {
            auto result = settings.Create();
            if (result.HasError()) {
                SPD_WARNING("Failed to create shape");
                return nullptr;
            }
            return result.Get();
        };

        switch (col.type) {
        case Collider::ColliderType::Box: {
            Math::Vec3 halfExtents = std::get<Collider::BoxColliderData>(col.data).halfExtents;
            JPH::BoxShapeSettings s(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z));
            
            base = CreateShape(s);
        } break;
        case Collider::ColliderType::Sphere: {
            JPH::SphereShapeSettings s(std::get<Collider::SphereColliderData>(col.data).radius);

            base = CreateShape(s);
        } break;
        case Collider::ColliderType::Capsule: {
            auto& data = std::get<Collider::CapsuleColliderData>(col.data);
            JPH::CapsuleShapeSettings s(data.height * 0.5, data.radius);

            base = CreateShape(s);
        } break;
        case Collider::ColliderType::Cylinder: {
            auto& data = std::get<Collider::CylinderColliderData>(col.data);
            JPH::CylinderShapeSettings s(data.height * 0.5, data.radius);

            base = CreateShape(s);
        } break;
        default:
            return;
        }

        JPH::ShapeRefC finalShape = base;
        if (!col.center.Zero()) {
            JPH::RotatedTranslatedShapeSettings rt(
                JPH::Vec3(col.center.x, col.center.y, col.center.z),
                JPH::Quat::sIdentity(),
                base
            );
            finalShape = CreateShape(rt);
            if (!finalShape) return;
        }

        m_shapes[entityLUID] = finalShape;
    }

    void PhysicsManager::RemoveShape(const uint64_t& entityLUID) {

    }
}
