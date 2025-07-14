#pragma once

#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>

//#include <Jolt/Core/Factory.h>

namespace NANOEngine::Physics {

    class PhysicsManager {
    public:
        static void Init();
        static void Update(float dt);
        static void Shutdown();

        static JPH::BodyID CreateBody(const JPH::BodyCreationSettings& settings);
        static void DestroyBody(JPH::BodyID id);

        static JPH::PhysicsSystem* GetPhysicsSystem();

    private:
        static std::unique_ptr<JPH::Factory> s_Factory;
        static std::unique_ptr<JPH::PhysicsSystem> s_PhysicsSystem;
        static std::unique_ptr<JPH::TempAllocatorImpl> s_TempAllocator;
        static std::unique_ptr<JPH::JobSystemThreadPool> s_JobSystem;
    };

}