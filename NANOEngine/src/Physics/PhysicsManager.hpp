#pragma once

#include <memory>
#include <array>
#include <unordered_map>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include "Core/Layers.hpp"
#include "ForceMode.hpp"

namespace NE::ECS {
    class ComponentManager;
}

namespace NE::ECS::Component {
    struct Collider;
    struct Transform;
    struct Rigidbody;
}

namespace NE::Math {
    struct Vec3;
}

namespace JPH {
    class Factory;
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemSingleThreaded;
    class BodyID;
}

namespace NE::Physics {
    class ObjectLayerPairFilterImpl;
    class BroadPhaseLayerInterfaceImpl;
    class ObjectVsBroadPhaseLayerFilterImpl;

    struct Ray;
    struct RaycastHit;
    
    class JoltDebugRenderer;

    class PhysicsManager {
    public:
        static PhysicsManager& GetInstance();

        void Init();
        void Update(double dt);
        void Shutdown();

        void OnPlay();
        void OnStop();

        void SetComponentManager(ECS::ComponentManager* cm);

        void CreateOrUpdateShape(const uint64_t entityLUID, const ECS::Component::Collider& col);
        void RemoveShape(const uint64_t entityLUID);

        void CreateBody(uint32_t entity, uint64_t entityLUID, const ECS::Component::Transform& t, const ECS::Component::Rigidbody& rb, const ECS::Component::Collider& col, uint8_t layerID);
        void CreateBody(uint32_t entity, uint64_t entityLUID, const ECS::Component::Transform& t, const ECS::Component::Collider& col, uint8_t layerID);
        void DestroyBody(uint64_t entityLUID);

        void SyncBodiesToTransform(uint64_t entityLUID, ECS::Component::Transform& t) const;

        void DrawShapeGizmo(const uint64_t entityLUID, const ECS::Component::Transform& t, const ECS::Component::Collider& col);
        void DrawBodies();

        bool Raycast(Math::Vec3 origin, Math::Vec3 direction, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);
        bool Raycast(Ray ray, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);
		bool SphereCast(Math::Vec3 origin, float radius, Math::Vec3 direction, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);
        bool SphereCast(Ray ray, float radius, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);

        void AddForce(uint64_t entityLUID, Math::Vec3 force, ForceMode forceMode = ForceMode::Force);

        Math::Vec3 GetLinearVelocity(uint64_t entityLUID) const;
        void SetLinearVelocity(uint64_t entityLUID, const Math::Vec3& velocity);
        Math::Vec3 GetAngularVelocity(uint64_t entityLUID) const;
        void SetAngularVelocity(uint64_t entityLUID, const Math::Vec3& angularVelocity);

    private:
        ECS::ComponentManager* m_componentManager = nullptr;

        std::unique_ptr<JPH::Factory> m_factory;
        std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
        std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
        std::unique_ptr<JPH::JobSystemSingleThreaded> m_jobSystem;

        std::array<Core::LayerMask, Core::MAX_LAYERS> m_collisionMatrix{};
        std::unique_ptr<ObjectLayerPairFilterImpl> m_objectLayerPairFilter;

        std::unique_ptr<BroadPhaseLayerInterfaceImpl> m_bpLayerInterface;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objectVsBpFilter;

        std::unique_ptr<JoltDebugRenderer> m_debugRenderer;

        std::unordered_map<uint64_t, JPH::ShapeRefC> m_shapes;
        std::unordered_map<uint64_t, JPH::BodyID> m_bodies;

        // to move to settings
        float m_fixedDt = 1.0f / 60.0f;
        int   m_collisionSteps = 1;
        float m_accumulator = 0.0f;
        float m_maxFrameTime = 0.25f;
        float m_alpha = 0.0f; // interpolation alpha for rendering
    };

}
