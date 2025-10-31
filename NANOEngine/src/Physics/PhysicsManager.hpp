#pragma once

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

//#include <Jolt/Core/Factory.h>
#include "../ECS/Core/Entity.hpp"


using namespace NE::ECS;

namespace NE::Physics {

    class PhysicsManager {
    public:
        static void Init();
      static void Update(float dt);
        static void Shutdown();

      static void ActivateBodies();
        static void DeactivateBodies();

        static uint32_t CreateBody(const JPH::BodyCreationSettings& settings);
     static void DestroyBody(uint32_t index);



        // === Velocity and Force Methods ===
        
        static Math::Vec3 GetLinearVelocity(uint32_t bodyID);
     static void SetLinearVelocity(uint32_t bodyID, const Math::Vec3& velocity);
        static void AddForce(uint32_t bodyID, const Math::Vec3& force);
        static void AddImpulse(uint32_t bodyID, const Math::Vec3& impulse);

        static JPH::PhysicsSystem* GetPhysicsSystem();
    static std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> s_shapeMap;

        static void SetTransform(uint32_t index, const Math::Vec3& position, const Math::Vec3& rotation);
        static void GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation);

        // For changing settings
        static void SetMotionType(uint32_t bodyid, JPH::EMotionType motionType);
        static JPH::EMotionType GetMotionType(uint32_t bodyid);

        static JPH::PhysicsSystem* GetPhysicsSystem();
        static std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> s_shapeMap;

        // Collider
        static uint32_t CreateBoxBody(const Math::Vec3& pos,
            const Math::Vec3& rot,
            const Math::Vec3& size,
            JPH::EMotionType motionType);

        static void UpdateBoxSize(uint32_t bodyID, const Math::Vec3& newSize);

        static uint32_t CreateSphereBody(const Math::Vec3& pos,
            const Math::Vec3& rot,
            float radius,
            JPH::EMotionType motionType);

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
        static void TestPhysicsSetup();  // Add this


    private:
        // swap manual tracking to jolt in built
        //static std::vector<JPH::BodyID> s_BodyIDs;
        //static std::unordered_map<uint32_t, size_t> s_BodyIndexMap; // Maps bodyID to index in s_BodyIDs
        static std::unique_ptr<JPH::Factory> s_Factory;
        static std::unique_ptr<JPH::PhysicsSystem> s_PhysicsSystem;
        static std::unique_ptr<JPH::TempAllocatorImpl> s_TempAllocator;
        static std::unique_ptr<JPH::JobSystemThreadPool> s_JobSystem;

        static std::unordered_map<Entity, uint32_t> s_EntityToBodyMap;

    };

}