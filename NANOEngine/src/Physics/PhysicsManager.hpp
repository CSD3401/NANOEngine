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
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include "Math/Vec3.hpp"
#include "ECS/Core/Entity.hpp"
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

namespace NE::ECS::Component {
    struct Collider;
}

namespace NE::Physics {
    class PhysicsManager {
    public:
        static PhysicsManager& GetInstance();

        void Init();
        void Update(float dt);
        void Shutdown();
        void Reset();

        void CreateOrUpdateShape(const uint64_t& entityLUID, const ECS::Component::Collider& col);
        void RemoveShape(const uint64_t& entityLUID);
    private:
        JPH::Factory m_factory{};
        JPH::PhysicsSystem m_physicsSystem{};
        std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
        std::unique_ptr<JPH::JobSystemSingleThreaded> m_jobSystem;

        std::unordered_map<uint64_t, JPH::ShapeRefC> m_shapes;
    };

}
