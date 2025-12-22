#include "PhysicsManager.hpp"

#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
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

// debug
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include "Core/SpdLogger.hpp"

#include "ECS/Components/Collider.hpp"
#include "ECS/Components/Rigidbody.hpp"
#include "ECS/Components/Transform.hpp"
#include "ObjectLayerPairFilterImpl.hpp"
#include "BroadPhaseLayerInterfaceImpl.hpp"
#include "ObjectVsBroadPhaseLayerFilterImpl.hpp"

namespace NE::Physics {
    namespace {
        // here for now to move to JoltMath
        JPH::Vec3 ToJoltVec3(const Math::Vec3& v) {
            return JPH::Vec3(v.x, v.y, v.z);
        }

        JPH::RMat44 ToJoltRMat44(const Math::Vec3& worldPos) {
            return JPH::RMat44::sTranslation(ToJoltVec3(worldPos));
        }

        JPH::EMotionType ToMotionType(const ECS::Component::Rigidbody& rb) {
            return rb.isKinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
        }

        JPH::ObjectLayer ToObjectLayer(uint8_t layerId, JPH::EMotionType motionType) {
            uint8_t baseLayer = layerId % 32;

            if (motionType != JPH::EMotionType::Static) {
                return static_cast<JPH::ObjectLayer>(baseLayer + 32);
            }

            return static_cast<JPH::ObjectLayer>(baseLayer);
        }

        NE::Math::Vec3 ToEngineVec3(const JPH::RVec3& v) {
            return NE::Math::Vec3(v.GetX(), v.GetY(), v.GetZ());
        }

        NE::Math::Vec3 JQuatToDegreeEuler(const JPH::Quat& q) {
            JPH::Vec3 angles = q.GetEulerAngles();
            return NE::Math::Vec3(JPH::RadiansToDegrees(angles.GetX()), JPH::RadiansToDegrees(angles.GetY()), JPH::RadiansToDegrees(angles.GetZ()));
        }
    }

    PhysicsManager& PhysicsManager::GetInstance() {
        static PhysicsManager instance;
        return instance;
    }

    void PhysicsManager::Init() {
        JPH::RegisterDefaultAllocator();
        m_factory = std::make_unique<JPH::Factory>();
        JPH::Factory::sInstance = m_factory.get();
        JPH::RegisterTypes();

        m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
        m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        m_jobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);

        for (int a = 0; a < Core::MAX_LAYERS; ++a)
            m_collisionMatrix[a] = ~Core::LayerMask(0);

        m_objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>(m_collisionMatrix);

        m_bpLayerInterface = std::make_unique<BroadPhaseLayerInterfaceImpl>();
        m_objectVsBpFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();

        m_debugRenderer = std::make_unique<JoltDebugRenderer>();
        JPH::DebugRenderer::sInstance = m_debugRenderer.get();
        m_debugRenderer->Init();

        const uint32_t maxBodies = 8192;
        const uint32_t numBodyMutexes = 0;
        const uint32_t maxBodyPairs = 65536;
        const uint32_t maxContactConstraints = 10240;

        m_physicsSystem->Init(
            maxBodies,
            numBodyMutexes,
            maxBodyPairs,
            maxContactConstraints,
            *m_bpLayerInterface,
            *m_objectVsBpFilter,
            *m_objectLayerPairFilter
        );

        m_physicsSystem->SetGravity(JPH::Vec3(0.f, -9.81f, 0.f));
    }

    void PhysicsManager::Update(double dt) {
        if (!m_physicsSystem)
            return;

        if (dt > m_maxFrameTime)
            dt = m_maxFrameTime;

        m_accumulator += dt;

        while (m_accumulator >= m_fixedDt) {
            const JPH::EPhysicsUpdateError err = m_physicsSystem->Update(
                m_fixedDt,
                m_collisionSteps,
                m_tempAllocator.get(),
                m_jobSystem.get()
            );

            // (Optional) handle err; in practice you can log it
            (void)err;

            m_accumulator -= m_fixedDt;
        }
    }

    void PhysicsManager::Shutdown() {
        m_physicsSystem.reset();
        m_jobSystem.reset();
        m_tempAllocator.reset();

        JPH::UnregisterTypes();
        JPH::Factory::sInstance = nullptr;
        m_factory.reset();
    }

    // TODO split create and update
    void PhysicsManager::CreateOrUpdateShape(const uint64_t entityLUID, const ECS::Component::Collider& col) {
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

        if (!base) return;

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

    void PhysicsManager::RemoveShape(const uint64_t entityLUID) {
        m_shapes.erase(entityLUID);
    }

    void PhysicsManager::CreateBody(uint64_t luid, const ECS::Component::Transform& t, 
        const ECS::Component::Rigidbody& rb, const ECS::Component::Collider& col, uint8_t layerID) {

        auto itShape = m_shapes.find(luid);
        if (itShape == m_shapes.end() || !itShape->second)
            return;

        const JPH::ShapeRefC& shape = itShape->second;

        const Math::Vec3 pos = t.worldMatrix.GetTranslation();
        const JPH::RVec3 jPos((double)pos.x, (double)pos.y, (double)pos.z);
        const JPH::Quat jRot = JPH::Quat::sEulerAngles({
            JPH::DegreesToRadians(t.localRotationEuler.x),
            JPH::DegreesToRadians(t.localRotationEuler.y),
            JPH::DegreesToRadians(t.localRotationEuler.z) }
        );

        const JPH::EMotionType motion = ToMotionType(rb);
        const JPH::ObjectLayer objLayer = ToObjectLayer(layerID, motion);

        JPH::BodyCreationSettings settings(
            shape,
            jPos,
            jRot,
            motion,
            objLayer
        );

        settings.mGravityFactor = rb.useGravity ? 1.0f : 0.0f;

        settings.mLinearDamping = rb.linearDamping;
        settings.mAngularDamping = rb.angularDamping;

        if (motion == JPH::EMotionType::Dynamic) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = rb.mass;
        }

        JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();
        JPH::Body* body = bi.CreateBody(settings);
        if (!body) return;

        const JPH::BodyID id = body->GetID();
        bi.AddBody(id, JPH::EActivation::Activate);

        m_bodies[luid] = id;
    }

    void PhysicsManager::CreateBody(uint64_t luid, const ECS::Component::Transform& t, 
        const ECS::Component::Collider& col, uint8_t layerID) {

        auto itShape = m_shapes.find(luid);
        if (itShape == m_shapes.end() || !itShape->second)
            return;

        const JPH::ShapeRefC& shape = itShape->second;

        const Math::Vec3 pos = t.worldMatrix.GetTranslation();
        const JPH::RVec3 jPos((double)pos.x, (double)pos.y, (double)pos.z);
        const JPH::Quat jRot = JPH::Quat::sEulerAngles({
            JPH::DegreesToRadians(t.localRotationEuler.x),
            JPH::DegreesToRadians(t.localRotationEuler.y),
            JPH::DegreesToRadians(t.localRotationEuler.z) }
        );

        const JPH::EMotionType motion = JPH::EMotionType::Static;
        const JPH::ObjectLayer objLayer = ToObjectLayer(layerID, motion);

        JPH::BodyCreationSettings settings(
            shape,
            jPos,
            jRot,
            motion,
            objLayer
        );

        JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();
        JPH::Body* body = bi.CreateBody(settings);
        if (!body) return;

        const JPH::BodyID id = body->GetID();
        bi.AddBody(id, JPH::EActivation::DontActivate);

        m_bodies[luid] = id;
    }

    void PhysicsManager::DestroyBody(uint64_t luid) {
        JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

        auto& id = m_bodies.at(luid);
        bi.RemoveBody(id);
        bi.DestroyBody(id);
    }

    void PhysicsManager::SyncBodiesToTransform(uint64_t luid, ECS::Component::Transform& t) const {
        JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

        auto& bodyID = m_bodies.at(luid);
        t.localPosition = ToEngineVec3(bi.GetPosition(bodyID));
        t.localRotationEuler = JQuatToDegreeEuler(bi.GetRotation(bodyID));

        t.isDirty = true;
    }

    void PhysicsManager::DrawShapeGizmo(const uint64_t entityLUID, const ECS::Component::Transform& t) {
        auto& shapeSettings = m_shapes.at(entityLUID);

        JPH::RMat44 com = ToJoltRMat44(t.worldMatrix.GetTranslation());

        shapeSettings->Draw(m_debugRenderer.get(), com, JPH::Vec3::sReplicate(1.f), JPH::Color::sGreen, false, true);
    }

    void PhysicsManager::DrawBodies() {

        //JPH::BodyManager::DrawSettings ds;
        //ds.mDrawShape = true;
        //ds.mDrawShapeWireframe = true;

        //m_physicsSystem->DrawBodies(ds, m_debugRenderer.get());
        //m_physicsSystem->DrawConstraints(ds, m_debugRenderer.get());
    }
    
    void PhysicsManager::OnPlay() {

    }

    void PhysicsManager::OnStop() {
        JPH::BodyInterface& bi = m_physicsSystem->GetBodyInterface();

        for (auto& [luid, id] : m_bodies) {
            bi.RemoveBody(id);
            bi.DestroyBody(id);
        }
        m_bodies.clear();
    }
}
