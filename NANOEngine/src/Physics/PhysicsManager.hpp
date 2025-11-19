#pragma once

// Save and undefine problematic Windows macros before including Jolt
#ifdef AddJob
#define NANOENGINE_ADDJOB_DEFINED
#pragma push_macro("AddJob")
#undef AddJob
#endif

#ifdef AddJobs
#define NANOENGINE_ADDJOBS_DEFINED  
#pragma push_macro("AddJobs")
#undef AddJobs
#endif

#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "../Math/Vec3.hpp"
#include "../ECS/Core/Entity.hpp"
#include "PhysicsContactListener.hpp"

// Restore macros after Jolt includes
#ifdef NANOENGINE_ADDJOB_DEFINED
#pragma pop_macro("AddJob")
#undef NANOENGINE_ADDJOB_DEFINED
#endif

#ifdef NANOENGINE_ADDJOBS_DEFINED
#pragma pop_macro("AddJobs") 
#undef NANOENGINE_ADDJOBS_DEFINED
#endif

using namespace NE::ECS;

namespace NE::Physics {

    class PhysicsManager {
    public:
        static void Init();
        static void Update(float dt);
        static void Shutdown();

        static void ActivateBodies();
        static void ActivateBody(uint32_t bodyID);
        static void DeactivateBodies();
        static void DeactivateBody(uint32_t bodyID);

        static uint32_t CreateBody(const JPH::BodyCreationSettings& settings);
        static void DestroyBody(uint32_t index);

        // === Velocity and Force Methods ===

        static Math::Vec3 GetLinearVelocity(uint32_t bodyID);
        static void SetLinearVelocity(uint32_t bodyID, const Math::Vec3& velocity);
        static void AddForce(uint32_t bodyID, const Math::Vec3& force);
        static void AddImpulse(uint32_t bodyID, const Math::Vec3& impulse);

        // === Rotation Locking ===
        static void LockRotation(uint32_t bodyID, bool lockX, bool lockY, bool lockZ);

        // === Raycasting Methods ===

        struct RaycastHit {
            bool hasHit = false;
            Math::Vec3 point;
            Math::Vec3 normal;
            float distance = 0.0f;
            uint32_t bodyID = 0;
            Entity entity = 0;
        };

        static constexpr uint32_t LAYER_NON_MOVING = (1 << 0);  // Bit 0 = static objects
        static constexpr uint32_t LAYER_MOVING = (1 << 1);      // Bit 1 = dynamic objects
        static constexpr uint32_t LAYER_ALL = 0xFFFFFFFF;       // All layers

        // Updated raycast method signatures with layer filtering
        static RaycastHit Raycast(
            const Math::Vec3& origin,
            const Math::Vec3& direction,
            float maxDistance,
            uint32_t layerMask = LAYER_ALL  // Default: hit everything
        );

        static std::vector<RaycastHit> RaycastAll(
            const Math::Vec3& origin,
            const Math::Vec3& direction,
            float maxDistance,
            uint32_t layerMask = LAYER_ALL  // Default: hit everything
        );

        static Entity GetBodyEntity(uint32_t bodyID);
        static JPH::PhysicsSystem* GetPhysicsSystem();
        static std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> s_shapeMap;

        static void SetTransform(uint32_t index, const Math::Vec3& position, const Math::Vec3& rotation);
        static void GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation);
        static void SetGravityEnabled(uint32_t bodyID, bool enabled);

        // For changing settings
        static void SetMotionType(uint32_t bodyid, JPH::EMotionType motionType);
        static JPH::EMotionType GetMotionType(uint32_t bodyid);

        // Collider creation
        static uint32_t CreateBoxBody(const Math::Vec3& pos,
            const Math::Vec3& rot,
            const Math::Vec3& size,
            JPH::EMotionType motionType);

        static void UpdateBoxSize(uint32_t bodyID, const Math::Vec3& newSize);

        static uint32_t CreateSphereBody(const Math::Vec3& pos,
            const Math::Vec3& rot,
            float radius,
            JPH::EMotionType motionType);

        static uint32_t CreateMeshShape(
            std::string meshID,
            const std::vector<Math::Vec3>& vertices,
            const std::vector<uint32_t>& indices);


        static void UpdateSphereRadius(uint32_t bodyID, float newRadius);

        static uint32_t CreateCapsuleBody(const Math::Vec3& pos, const Math::Vec3& rot,
            float halfHeight, float radius, JPH::EMotionType motionType);

        static void RenderAllBodyShapes();
        static void RenderBodyShape(const JPH::Body& body);

        // Entity-physics mapping
        static void RegisterEntityBody(Entity entity, uint32_t bodyID);
        static void UnregisterEntityBody(Entity entity);
        static uint32_t GetEntityBodyId(Entity entity);

        static bool EntityHasPhysicsBody(Entity entity);
        static void TestPhysicsSetup();

        // Collision Callbacks
        static void RegisterCollisionEnterCallback(PhysicsContactListener::CollisionCallback callback);
        static void RegisterCollisionStayCallback(PhysicsContactListener::CollisionCallback callback);
        static void RegisterCollisionExitCallback(PhysicsContactListener::CollisionCallback callback);

        static void ClearAllBodies();

    private:
        // swap manual tracking to jolt in built
        //static std::vector<JPH::BodyID> s_BodyIDs;
        //static std::unordered_map<uint32_t, size_t> s_BodyIndexMap; // Maps bodyID to index in s_BodyIDs
        static std::unique_ptr<JPH::Factory> s_Factory;
        static std::unique_ptr<JPH::PhysicsSystem> s_PhysicsSystem;
        static std::unique_ptr<JPH::TempAllocatorImpl> s_TempAllocator;
        static std::unique_ptr<JPH::JobSystemThreadPool> s_JobSystem;
        static std::unique_ptr<PhysicsContactListener> s_ContactListener;

        static std::unordered_map<Entity, uint32_t> s_EntityToBodyMap;
    };
}