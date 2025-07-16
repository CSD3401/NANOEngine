#pragma once

#include <memory>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/PhysicsSystem.h>
//#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include "../Math/Vec3.hpp"

//#include <Jolt/Core/Factory.h>

namespace NANOEngine::Physics {

    class PhysicsManager {
    public:
        static void Init();
        static void Update(float dt);
        static void Shutdown();

        static JPH::BodyID CreateBody(const JPH::BodyCreationSettings& settings);
        static void DestroyBody(uint32_t index);

        static void GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation);

        static JPH::PhysicsSystem* GetPhysicsSystem();
        static std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> s_shapeMap;
    private:
        static std::unique_ptr<JPH::Factory> s_Factory;
        static std::unique_ptr<JPH::PhysicsSystem> s_PhysicsSystem;
        static std::unique_ptr<JPH::TempAllocatorImpl> s_TempAllocator;
        static std::unique_ptr<JPH::JobSystemThreadPool> s_JobSystem;
    };

}