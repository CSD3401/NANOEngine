#pragma once

#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "../Math/Vec3.hpp"

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

		static void SetTransform(uint32_t index, const Math::Vec3& position, const Math::Vec3& rotation);
   static void GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation);

      static void SetMotionType(uint32_t bodyid, JPH::EMotionType motionType);

        // === Velocity and Force Methods ===
        
        static Math::Vec3 GetLinearVelocity(uint32_t bodyID);
     static void SetLinearVelocity(uint32_t bodyID, const Math::Vec3& velocity);
        static void AddForce(uint32_t bodyID, const Math::Vec3& force);
        static void AddImpulse(uint32_t bodyID, const Math::Vec3& impulse);

        static JPH::PhysicsSystem* GetPhysicsSystem();
    static std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> s_shapeMap;
    private:
    static std::vector<JPH::BodyID> s_BodyIDs;
		static std::unordered_map<uint32_t, size_t> s_BodyIndexMap;
     static std::unique_ptr<JPH::Factory> s_Factory;
     static std::unique_ptr<JPH::PhysicsSystem> s_PhysicsSystem;
   static std::unique_ptr<JPH::TempAllocatorImpl> s_TempAllocator;
        static std::unique_ptr<JPH::JobSystemThreadPool> s_JobSystem;
    };

}