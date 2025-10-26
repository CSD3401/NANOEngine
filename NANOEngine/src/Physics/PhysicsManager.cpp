#include "PhysicsManager.hpp"
#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <iostream>

namespace NE::Physics {

    // Object layers used by the engine
    namespace Layers {
        inline constexpr JPH::ObjectLayer NON_MOVING = 0;
        inline constexpr JPH::ObjectLayer MOVING = 1;
        inline constexpr JPH::ObjectLayer NUM_LAYERS = 2;
    }

    // Broad phase layers
    namespace BPLayers {
        inline constexpr JPH::BroadPhaseLayer NON_MOVING(0);
        inline constexpr JPH::BroadPhaseLayer MOVING(1);
        inline constexpr unsigned int NUM_LAYERS = 2;
    }

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        BPLayerInterfaceImpl() {
            mObjectToBroadPhase[Layers::NON_MOVING] = BPLayers::NON_MOVING;
            mObjectToBroadPhase[Layers::MOVING] = BPLayers::MOVING;
        }

        virtual unsigned int GetNumBroadPhaseLayers() const override { return BPLayers::NUM_LAYERS; }

        virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
            JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
            return mObjectToBroadPhase[inLayer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
            switch ((JPH::BroadPhaseLayer::Type)inLayer.GetValue()) {
            case 0: return "NON_MOVING";
            case 1: return "MOVING";
            default: return "INVALID";
            }
        }
#endif
    private:
        JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
    };

    class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
    };

    std::vector<JPH::BodyID> PhysicsManager::s_BodyIDs;
    std::unordered_map<uint32_t, size_t> PhysicsManager::s_BodyIndexMap;
    std::unique_ptr<JPH::Factory> PhysicsManager::s_Factory;
    std::unique_ptr<JPH::PhysicsSystem> PhysicsManager::s_PhysicsSystem;
    std::unique_ptr<JPH::TempAllocatorImpl> PhysicsManager::s_TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> PhysicsManager::s_JobSystem;

    static BPLayerInterfaceImpl s_BPLayerInterface;
    static ObjectVsBroadPhaseLayerFilterImpl s_ObjectVsBroadPhaseLayerFilter;
    static ObjectLayerPairFilterImpl s_ObjectLayerPairFilter;

    std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> PhysicsManager::s_shapeMap;

    static JoltDebugRenderer g_joltDebugRenderer;

#pragma region test
    //static void TestDebugDraw()
    //{
    //    using DR = JPH::DebugRenderer;

    //    // make 3 points (vertices) that form a small triangle
    //    DR::Vertex verts[3] = {};
    //    verts[0].mPosition = JPH::Float3(0.0f, 0.0f, 0.0f); // bottom-left corner
    //    verts[1].mPosition = JPH::Float3(1.0f, 0.0f, 0.0f); // bottom-right corner
    //    verts[2].mPosition = JPH::Float3(0.0f, 1.0f, 0.0f); // top corner

    //    // "indices" tell how to connect the points into a triangle
    //    const JPH::uint32 indices[3] = { 0, 1, 2 };

    //    // ask Jolt to bundle this vertex + index data into a "triangle batch"
    //    DR::Batch batch = g_joltDebugRenderer.CreateTriangleBatch(verts, 3, indices, 3);
    //    if (!batch) return; 

    //    // compute the triangle’s bounding box (needed for culling)
    //    const JPH::AABox bounds = DR::sCalculateBounds(verts, 3);
    //    
    //    // wrap the batch + bounds into a "Geometry" object
    //    // jolt uses this to store all info it needs to render a shape
    //    DR::GeometryRef geom = new DR::Geometry(batch, bounds);

    //    // scaling factor
    //    const float lodScaleSq = 1.0f;

    //    // 1. draw a white filled triangle at position(0, 0, 0)
    //    JPH::RMat44 Msolid = JPH::RMat44::sTranslation(JPH::RVec3(0.0, 0.0, 0.0));  // RMat44 is just a 4x4 transform matrix — here we use identity (no move)
    //    g_joltDebugRenderer.DrawGeometry(
    //        Msolid,
    //        geom->mBounds.Transformed(Msolid),
    //        lodScaleSq,
    //        JPH::Color::sWhite,
    //        geom,
    //        DR::ECullMode::Off,
    //        DR::ECastShadow::Off,
    //        DR::EDrawMode::Solid
    //    );

    //    // 2. draw a red wireframe triangle, a little to the right
    //    JPH::RMat44 Mwire = JPH::RMat44::sTranslation(JPH::RVec3(1.25, 0.0, 0.0));
    //    g_joltDebugRenderer.DrawGeometry(
    //        Mwire,
    //        geom->mBounds.Transformed(Mwire),
    //        lodScaleSq,
    //        JPH::Color::sRed,
    //        geom,
    //        DR::ECullMode::Off,
    //        DR::ECastShadow::Off,
    //        DR::EDrawMode::Wireframe
    //    );

    //    // 3. test draw sphere, capsule, cylinder
    //    // sphere
    //    {
    //        // solid
    //        g_joltDebugRenderer.DrawSphere(
    //            JPH::RVec3(0.0, 0.0, 1.5),   // center position (x, y, z)
    //            0.5f,                        // how big the ball is
    //            JPH::Color::sGreen,          // its color
    //            DR::ECastShadow::Off,        // no shadows
    //            DR::EDrawMode::Solid         // make it solid (not just a wireframe)
    //        );

    //        // wireframe
    //        // Use DrawUnitSphere with a transform: scale = radius, translation = position
    //        JPH::RMat44 T = JPH::RMat44::sTranslation(JPH::RVec3(1.25, 0.0, 1.5));
    //        JPH::RMat44 S = JPH::RMat44::sScale(JPH::Vec3(0.5f, 0.5f, 0.5f));
    //        JPH::RMat44 M = T * S;
    //        g_joltDebugRenderer.DrawUnitSphere(
    //            M,
    //            JPH::Color::sRed,
    //            DR::ECastShadow::Off,
    //            DR::EDrawMode::Wireframe
    //        );
    //    }

    //    // capsule
    //    {
    //        // solid
    //        g_joltDebugRenderer.DrawCapsule(
    //            JPH::RMat44::sTranslation(JPH::RVec3(3.0, 0.0, 1.5)), // place it further right
    //            0.5f,                 // half the height of the cylinder part
    //            0.25f,                // radius (how fat it is)
    //            JPH::Color::sYellow,  // color
    //            DR::ECastShadow::Off,
    //            DR::EDrawMode::Solid  // solid fill
    //        );

    //        // wireframe
    //        g_joltDebugRenderer.DrawCapsule(
    //            JPH::RMat44::sTranslation(JPH::RVec3(4.0, 0.0, 1.5)),
    //            0.5f,
    //            0.25f,
    //            JPH::Color::sOrange,
    //            DR::ECastShadow::Off,
    //            DR::EDrawMode::Wireframe
    //        );
    //    }

    //    // cylinder
    //    {
    //        // solid
    //        g_joltDebugRenderer.DrawCylinder(
    //            JPH::RMat44::sTranslation(JPH::RVec3(5.5, 0.0, 1.5)), // push it further right
    //            0.5f,                // half-height (so total height = 1.0)
    //            0.3f,                // radius
    //            JPH::Color::sCyan,   // color
    //            DR::ECastShadow::Off,
    //            DR::EDrawMode::Solid // solid fill
    //        );

    //        // wireframe
    //        g_joltDebugRenderer.DrawCylinder(
    //            JPH::RMat44::sTranslation(JPH::RVec3(6.5, 0.0, 1.5)),
    //            0.5f,
    //            0.3f,
    //            JPH::Color::sBlue,
    //            DR::ECastShadow::Off,
    //            DR::EDrawMode::Wireframe
    //        );
    //    }

    //    // (Optional) Draw a few extras if you want to confirm orientation:
    //    g_joltDebugRenderer.DrawLine(JPH::RVec3(0, 0, 0), JPH::RVec3(1, 0, 0), JPH::Color::sRed);    // X-axis
    //    g_joltDebugRenderer.DrawLine(JPH::RVec3(0, 0, 0), JPH::RVec3(0, 1, 0), JPH::Color::sGreen);  // Y-axis
    //    g_joltDebugRenderer.DrawLine(JPH::RVec3(0, 0, 0), JPH::RVec3(0, 0, 1), JPH::Color::sBlue);   // Z-axis
    //}
#pragma endregion

    void PhysicsManager::Init() {
        JPH::RegisterDefaultAllocator();

        s_Factory = std::make_unique<JPH::Factory>();
        JPH::Factory::sInstance = s_Factory.get();
        JPH::RegisterTypes();

        JPH::DebugRenderer::sInstance = &g_joltDebugRenderer;
        g_joltDebugRenderer.Init();

        s_TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
        s_JobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, -1);

        s_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
        const unsigned int cMaxBodies = 1024;
        const unsigned int cNumBodyMutexes = 0; // Auto detect
        const unsigned int cMaxBodyPairs = 1024;
        const unsigned int cMaxContactConstraints = 1024;
        s_PhysicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
            s_BPLayerInterface, s_ObjectVsBroadPhaseLayerFilter, s_ObjectLayerPairFilter);
    }

    void PhysicsManager::Update(float dt) {
        if (!s_PhysicsSystem)
            return;

#pragma region test
        // test DrawLine() and DrawTriangle() function
        //TestDebugDraw();

        s_PhysicsSystem->Update(dt, 1, s_TempAllocator.get(), s_JobSystem.get());

        JPH::BodyManager::DrawSettings drawSettings;
        //drawSettings.mDrawShape = true;
        drawSettings.mDrawShape = false; // must be false to use our own jolt implementations
        drawSettings.mDrawBoundingBox = true;  // show body AABBs 
        s_PhysicsSystem->DrawBodies(drawSettings, &g_joltDebugRenderer);
#pragma endregion
    }

    void PhysicsManager::Shutdown() {
        s_PhysicsSystem.reset();
        s_TempAllocator.reset();
        s_JobSystem.reset();

        JPH::UnregisterTypes();
        JPH::Factory::sInstance = nullptr;
        s_Factory.reset();
    }

    void PhysicsManager::ActivateBodies()
    {
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        bodyInterface.ActivateBodies(s_BodyIDs.data(), static_cast<int>(s_BodyIDs.size()));
    }

    void PhysicsManager::DeactivateBodies()
    {
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        bodyInterface.DeactivateBodies(s_BodyIDs.data(), static_cast<int>(s_BodyIDs.size()));
    }

    uint32_t PhysicsManager::CreateBody(const JPH::BodyCreationSettings& settings) {
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        JPH::BodyID bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        s_BodyIDs.push_back(bodyID);
		s_BodyIndexMap[bodyID.GetIndexAndSequenceNumber()] = s_BodyIDs.size() - 1;
        return bodyID.GetIndexAndSequenceNumber();
    }

    void PhysicsManager::DestroyBody(uint32_t index) {
        //JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        //bodyInterface.RemoveBody(id);
        //bodyInterface.DestroyBody(id);
        JPH::BodyID id(index);
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        bodyInterface.RemoveBody(id);
        bodyInterface.DestroyBody(id);
    }
    
    void PhysicsManager::SetTransform(uint32_t index, const Math::Vec3& position, const Math::Vec3& rotation)
    {
        JPH::BodyID id(index);
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();

        JPH::RVec3 joltPos(position.x, position.y, position.z);

        JPH::Quat joltRot = JPH::Quat::sEulerAngles({
            JPH::DegreesToRadians(rotation.x),
            JPH::DegreesToRadians(rotation.y),
            JPH::DegreesToRadians(rotation.z) }
        );

        // Actually move the body
        bodyInterface.SetPositionAndRotation(id, joltPos, joltRot, JPH::EActivation::DontActivate);
        printf("PhysicsManager: Set transform for body ID %d to position (%f, %f, %f) and rotation (%f, %f, %f)\n",
			index, position.x, position.y, position.z, rotation.x, rotation.y, rotation.z);
    }

    //void PhysicsManager::GetTransform(JPH::BodyID id, Math::Vec3& position, Math::Vec3& rotation) {
    //    JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
    //    JPH::RVec3 pos;
    //    JPH::Quat rot;
    //    bodyInterface.GetPositionAndRotation(id, pos, rot);
    //    position = { static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ()) };
    //    JPH::Vec3 angles = rot.GetEulerAngles();
    //    rotation = { JPH::RadiansToDegrees(angles.GetX()), JPH::RadiansToDegrees(angles.GetY()), JPH::RadiansToDegrees(angles.GetZ()) };
    //}

    void PhysicsManager::GetTransform(uint32_t index, Math::Vec3& position, Math::Vec3& rotation) {
        JPH::BodyID id(index);
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        JPH::RVec3 pos;
        JPH::Quat rot;
        bodyInterface.GetPositionAndRotation(id, pos, rot);
        position = { static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ()) };
        JPH::Vec3 angles = rot.GetEulerAngles();
        rotation = { JPH::RadiansToDegrees(angles.GetX()), JPH::RadiansToDegrees(angles.GetY()), JPH::RadiansToDegrees(angles.GetZ()) };
    }

    void PhysicsManager::SetMotionType(uint32_t bodyid, JPH::EMotionType motionType)
    {
        JPH::BodyID id(bodyid);
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        bodyInterface.SetMotionType(id, motionType, JPH::EActivation::DontActivate);
        bodyInterface.SetLinearVelocity(id, JPH::Vec3::sZero());
        bodyInterface.SetAngularVelocity(id, JPH::Vec3::sZero());

        auto it = s_BodyIndexMap.find(bodyid);
        if (it == s_BodyIndexMap.end())
            return; // not found

        size_t index = it->second;
        size_t lastIndex = s_BodyIDs.size() - 1;

        // If it's not the last element, swap with the last
        if (index != lastIndex) {
            JPH::BodyID lastID = s_BodyIDs[lastIndex];
            s_BodyIDs[index] = lastID;

            // Update the map entry for the moved element
            s_BodyIndexMap[lastID.GetIndexAndSequenceNumber()] = index;
        }

        // Remove last element
        s_BodyIDs.pop_back();

        // Remove from map
        s_BodyIndexMap.erase(it);

		printf("PhysicsManager: Set motion type for body ID %d to %d\n", bodyid, static_cast<int>(motionType));
    }

    JPH::PhysicsSystem* PhysicsManager::GetPhysicsSystem() {
        return s_PhysicsSystem.get();
    }

}