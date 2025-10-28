#include "PhysicsManager.hpp"
#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

// debug
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>

// end debug

#include <iostream>

namespace NE::Physics {

	// Self notes:
    // Body: Physics representation of that entity (collision, forces, movement)
    // Not to be confused with 
    // Entity: Your game object (has transform, model, logic)



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

    //std::vector<JPH::BodyID> PhysicsManager::s_BodyIDs;
    //std::unordered_map<uint32_t, size_t> PhysicsManager::s_BodyIndexMap;
    std::unique_ptr<JPH::Factory> PhysicsManager::s_Factory;
    std::unique_ptr<JPH::PhysicsSystem> PhysicsManager::s_PhysicsSystem;
    std::unique_ptr<JPH::TempAllocatorImpl> PhysicsManager::s_TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> PhysicsManager::s_JobSystem;
    std::unordered_map<NE::ECS::Entity, uint32_t> PhysicsManager::s_EntityToBodyMap;

    static BPLayerInterfaceImpl s_BPLayerInterface;
    static ObjectVsBroadPhaseLayerFilterImpl s_ObjectVsBroadPhaseLayerFilter;
    static ObjectLayerPairFilterImpl s_ObjectLayerPairFilter;

    std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> PhysicsManager::s_shapeMap;

    static JoltDebugRenderer g_joltDebugRenderer;

#pragma region test Jolt Physics Debug Draw
    static void TestJoltPhysicsDebugDraw()
    {
//        using DR = JPH::DebugRenderer;
//
//#pragma region test 1 - test basic draw functions
//        // 1. Test draw sphere, capsule, cylinder
//        std::cout << "[Test 1] Drawing primitive shapes (sphere, capsule, cylinder)..." << std::endl;
//
//        // sphere
//        {
//            // solid
//            g_joltDebugRenderer.DrawSphere(JPH::RVec3(0.0, 1.0, 0.0), 0.5f, JPH::Color::sGreen, DR::ECastShadow::Off, DR::EDrawMode::Solid);
//            std::cout << "  - Solid sphere (green) at (0, 1, 0)" << std::endl;
//
//            // wireframe
//            // Use DrawUnitSphere with a transform: scale = radius, translation = position
//            JPH::RMat44 T = JPH::RMat44::sTranslation(JPH::RVec3(1.25, 1.0, 0.0));
//            JPH::RMat44 S = JPH::RMat44::sScale(JPH::Vec3(0.5f, 0.5f, 0.5f));
//            JPH::RMat44 M = T * S;
//            g_joltDebugRenderer.DrawUnitSphere(M, JPH::Color::sRed, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);
//            std::cout << "  - Wireframe sphere (red) at (1.25, 1, 0)" << std::endl;
//        }
//
//        // capsule
//        {
//            // solid
//            g_joltDebugRenderer.DrawCapsule(JPH::RMat44::sTranslation(JPH::RVec3(3.0, 1.0, 0.0)), 0.5f, 0.25f, JPH::Color::sYellow, DR::ECastShadow::Off, DR::EDrawMode::Solid);
//            std::cout << "  - Solid capsule (yellow) at (3, 1, 0)" << std::endl;
//
//            // wireframe
//            g_joltDebugRenderer.DrawCapsule(JPH::RMat44::sTranslation(JPH::RVec3(4.0, 1.0, 0.0)), 0.5f, 0.25f, JPH::Color::sOrange, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);
//            std::cout << "  - Wireframe capsule (orange) at (4, 1, 0)" << std::endl;
//        }
//
//        // cylinder
//        {
//            // solid
//            g_joltDebugRenderer.DrawCylinder(JPH::RMat44::sTranslation(JPH::RVec3(5.5, 1.0, 0.0)), 0.5f, 0.3f, JPH::Color::sCyan, DR::ECastShadow::Off, DR::EDrawMode::Solid);
//            std::cout << "  - Solid cylinder (cyan) at (5.5, 1, 0)" << std::endl;
//
//            // wireframe
//            g_joltDebugRenderer.DrawCylinder(JPH::RMat44::sTranslation(JPH::RVec3(6.5, 1.0, 0.0)), 0.5f, 0.3f, JPH::Color::sBlue, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);
//            std::cout << "  - Wireframe cylinder (blue) at (6.5, 1, 0)" << std::endl;
//        }
//
//        //// Draw coordinate axes at origin for orientation reference
//        //g_joltDebugRenderer.DrawLine(JPH::RVec3(0, 0, 0), JPH::RVec3(1, 0, 0), JPH::Color::sRed);    // X-axis
//        //g_joltDebugRenderer.DrawLine(JPH::RVec3(0, 0, 0), JPH::RVec3(0, 1, 0), JPH::Color::sGreen);  // Y-axis
//        //g_joltDebugRenderer.DrawLine(JPH::RVec3(0, 0, 0), JPH::RVec3(0, 0, 1), JPH::Color::sBlue);   // Z-axis
//        std::cout << "  - Coordinate axes drawn at origin" << std::endl;
//#pragma endregion test 1 - test basic draw functions
//
//#pragma region test 2 - test DrawGeometry() function
//        // 2. Test draw geometry function
//        std::cout << "\n[Test 2] Drawing custom geometry (triangles)..." << std::endl;
//
//        // make 3 points (vertices) that form a small triangle
//        DR::Vertex verts[3] = {};
//        verts[0].mPosition = JPH::Float3(0.0f, 0.0f, 0.0f); // bottom-left corner
//        verts[1].mPosition = JPH::Float3(1.0f, 0.0f, 0.0f); // bottom-right corner
//        verts[2].mPosition = JPH::Float3(0.0f, 1.0f, 0.0f); // top corner
//
//        // "indices" tell how to connect the points into a triangle
//        const JPH::uint32 indices[3] = { 0, 1, 2 };
//
//        // ask Jolt to bundle this vertex + index data into a "triangle batch"
//        DR::Batch batch = g_joltDebugRenderer.CreateTriangleBatch(verts, 3, indices, 3);
//        if (!batch) return;
//
//        // compute the triangle's bounding box (needed for culling)
//        const JPH::AABox bounds = DR::sCalculateBounds(verts, 3);
//
//        // wrap the batch + bounds into a "Geometry" object
//        // jolt uses this to store all info it needs to render a shape
//        DR::GeometryRef geom = new DR::Geometry(batch, bounds);
//
//        // scaling factor
//        const float lodScaleSq = 1.0f;
//
//        // draw a white filled triangle at position(0, 0, 0)
//        JPH::RMat44 Msolid = JPH::RMat44::sTranslation(JPH::RVec3(0.0, 2.0, 0.0));
//        g_joltDebugRenderer.DrawGeometry(Msolid, geom->mBounds.Transformed(Msolid), lodScaleSq, JPH::Color::sWhite, geom, DR::ECullMode::Off, DR::ECastShadow::Off, DR::EDrawMode::Solid);
//        std::cout << "  - Solid triangle (white) at (0, 2.0, 0)" << std::endl;
//
//        // draw a red wireframe triangle, a little to the right
//        JPH::RMat44 Mwire = JPH::RMat44::sTranslation(JPH::RVec3(1.25, 2.0, 0.0));
//        g_joltDebugRenderer.DrawGeometry(Mwire, geom->mBounds.Transformed(Mwire), lodScaleSq, JPH::Color::sRed, geom, DR::ECullMode::Off, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);
//        std::cout << "  - Wireframe triangle (red) at (1.25, 2.0, 0)" << std::endl;
//#pragma endregion test 2 - test DrawGeometry() function
//
//#pragma region test 3 - test DrawBodies(), DrawShapes() function
//        // 3. Test draw bodies function
//        std::cout << "\n[Test 3] Drawing physics bodies via DrawBodies function..." << std::endl;
//
//        // Get physics system
//        JPH::PhysicsSystem* ps = NE::Physics::PhysicsManager::GetPhysicsSystem();
//        if (!ps) return;
//
//        // Spawn test bodies once
//        static bool spawned = false;
//        if (!spawned)
//        {
//            JPH::BodyInterface& bi = ps->GetBodyInterface();
//
//            // -- Floor (static)
//            {
//                JPH::RefConst<JPH::Shape> floor = new JPH::BoxShape(JPH::Vec3(10.0f, 0.2f, 10.0f));
//                JPH::BodyCreationSettings s(floor, JPH::RVec3(0, -0.2f, 0), JPH::Quat::sIdentity(), JPH::EMotionType::Static, 0);
//                bi.CreateAndAddBody(s, JPH::EActivation::DontActivate);
//                std::cout << "  - Created floor (static)" << std::endl;
//            }
//
//            // -- Box (dynamic)
//            {
//                JPH::RefConst<JPH::Shape> box = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
//                JPH::BodyCreationSettings s(box, JPH::RVec3(-2.0, 3.5, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, 1);
//                bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//                std::cout << "  - Created box (dynamic) at (-2, 3.5, 0)" << std::endl;
//            }
//
//            // -- Sphere (dynamic)
//            {
//                JPH::RefConst<JPH::Shape> sph = new JPH::SphereShape(0.5f);
//                JPH::BodyCreationSettings s(sph, JPH::RVec3(0.0, 3.5, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, 1);
//                bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//                std::cout << "  - Created sphere (dynamic) at (0, 3.5, 0)" << std::endl;
//            }
//
//            // -- Capsule (dynamic)
//            {
//                float halfHeight = 0.5f;
//                float radius = 0.25f;
//                JPH::RefConst<JPH::Shape> cap = new JPH::CapsuleShape(halfHeight, radius);
//                JPH::BodyCreationSettings s(cap, JPH::RVec3(2.0, 3.75, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, 1);
//                bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//                std::cout << "  - Created capsule (dynamic) at (2, 3.75, 0)" << std::endl;
//            }
//
//            // -- Cylinder (dynamic)
//            {
//                float halfHeight = 0.5f;
//                float radius = 0.3f;
//                JPH::RefConst<JPH::Shape> cyl = new JPH::CylinderShape(halfHeight, radius);
//                JPH::BodyCreationSettings s(cyl, JPH::RVec3(4.0, 3.5, 0.0), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, 1);
//                bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//                std::cout << "  - Created cylinder (dynamic) at (4, 3.5, 0)" << std::endl;
//            }
//
//            spawned = true;
//            std::cout << "  - All bodies spawned successfully" << std::endl;
//        }
//
//        // Draw all bodies in the physics system
//        JPH::BodyManager::DrawSettings drawSettings;
//        drawSettings.mDrawShape = true;                      // Draw solid shapes
//        drawSettings.mDrawShapeWireframe = true;             // Set true to overlay wireframe on shapes
//        drawSettings.mDrawBoundingBox = true;                // Set true to see axis-aligned bounding boxes (AABBs)
//        //drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::MotionTypeColor;  // Color: Static=gray, Dynamic=green (affected by forces), Kinematic=yellow (not affected by forces)
//        //drawSettings.mDrawCenterOfMassTransform = false;    // Draw tiny RGB axes at center of mass (can be hard to see as they are very small, need to scale down entity's size)
//        //drawSettings.mDrawWorldTransform = false;           // need to update body position in physics system to work
//        //drawSettings.mDrawVelocity = false;                 // Draw velocity vectors as arrows (only for dynamic bodies)
//        //drawSettings.mDrawGetSupportFunction = false;       // Advanced: draw support point in specified direction (not sure how this works)
//        //drawSettings.mDrawGetSupportingFace = false;        // Advanced: draw supporting face for collision detection (not sure how this works)
//
//        ps->DrawBodies(drawSettings, &g_joltDebugRenderer);
//        std::cout << "  - DrawBodies executed with current settings" << std::endl;
//#pragma endregion test 3 - test DrawBodies(), DrawShapes() function
//
//        std::cout << "\n[All tests completed]" << std::endl;
    }
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

        // Update before render
        s_PhysicsSystem->Update(dt, 1, s_TempAllocator.get(), s_JobSystem.get());

        // I'll leave this here first... - RF
        //JPH::BodyManager::DrawSettings drawSettings;
        ////drawSettings.mDrawShape = true;
        //drawSettings.mDrawShape = false; // must be false to use our own jolt implementations
        //drawSettings.mDrawBoundingBox = true;  // show body AABBs 
        //s_PhysicsSystem->DrawBodies(drawSettings, &g_joltDebugRenderer);

        RenderAllBodyShapes();

        // I commented out the draw code killing FPS ! RF
        TestJoltPhysicsDebugDraw();

    }

    void PhysicsManager::RenderAllBodyShapes()
    {
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        (void)bodyInterface; // suppress warning

        JPH::BodyIDVector bodyIDs;
        s_PhysicsSystem->GetBodies(bodyIDs);

        for (JPH::BodyID bodyID : bodyIDs) 
        {
            // This lock is for race condition NOT transform lock !
            JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), bodyID);
            if (lock.Succeeded()) 
            {
                const JPH::Body& body = lock.GetBody();

                //if (body.IsActive() || body.IsKinematic()) 
                {
                    RenderBodyShape(body);
                }
            }
        }
    }

    void PhysicsManager::RenderBodyShape(const JPH::Body& body)
    {
        const JPH::Shape* shape = body.GetShape();
        if (!shape)
            return;

        const JPH::RMat44 transform = body.GetWorldTransform();
        const JPH::Color color =
            body.IsDynamic() ? JPH::Color::sGreen : body.IsKinematic() ? JPH::Color::sRed : JPH::Color::sGrey;

        const JPH::EShapeSubType subType = shape->GetSubType();
		(void)subType; // suppress warning

        shape->Draw(
            &g_joltDebugRenderer,
            transform,
            JPH::Vec3::sReplicate(1.0f),
            color,
            false,
            true
        );

        return; //early

   //     switch (subType)
   //     {
   //     case JPH::EShapeSubType::Box:
   //     {
   //         //const JPH::BoxShape* boxShape = static_cast<const JPH::BoxShape*>(shape);
   //         //const JPH::Vec3 halfExtent = boxShape->GetHalfExtent();

   //         // //Create an AABox from the half extents
   //         //JPH::AABox box(-halfExtent, halfExtent); // From -halfExtent to +halfExtent

   //         //g_joltDebugRenderer.DrawBox(
   //         //    transform,
   //         //    box,
   //         //    color,
   //         //    DR::ECastShadow::Off,
   //         //    DR::EDrawMode::Wireframe
   //         //);
   //         //break;
   //     }

   //     case JPH::EShapeSubType::Sphere:
   //     {
   //         const JPH::SphereShape* sphereShape = static_cast<const JPH::SphereShape*>(shape);
   //         g_joltDebugRenderer.DrawSphere(
   //             transform.GetTranslation(),
   //             sphereShape->GetRadius(),
   //             color,
   //             DR::ECastShadow::Off,
   //             DR::EDrawMode::Wireframe
   //         );
   //         break;
   //     }

   //     case JPH::EShapeSubType::Capsule:
   //     {
   //         const JPH::CapsuleShape* capsuleShape = static_cast<const JPH::CapsuleShape*>(shape);
   //         g_joltDebugRenderer.DrawCapsule(
   //             transform,
   //             capsuleShape->GetHalfHeightOfCylinder(),
   //             capsuleShape->GetRadius(),
   //             color,
   //             DR::ECastShadow::Off,
   //             DR::EDrawMode::Wireframe
   //         );
   //         break;
   //     }

   //     case JPH::EShapeSubType::Cylinder:
   //     {
   //         const JPH::CylinderShape* cylinderShape = static_cast<const JPH::CylinderShape*>(shape);
   //         g_joltDebugRenderer.DrawCylinder(
   //             transform,
   //             cylinderShape->GetHalfHeight(),
   //             cylinderShape->GetRadius(),
   //             color,
   //             DR::ECastShadow::Off,
   //             DR::EDrawMode::Wireframe
   //         );
   //         break;
   //     }

   //     default:
   //     {
   //         // For complex shapes, use built-in shape drawing (safe fallback)
   //         // Untested default code - RF
   //         shape->Draw(
   //             &g_joltDebugRenderer,
   //             transform,
   //             JPH::Vec3::sReplicate(1.0f),
   //             color,
   //             false, 
   //             true
   //         );
   //         break;
   //     }
   //     }
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
        JPH::BodyIDVector allBodies;
        s_PhysicsSystem->GetBodies(allBodies);

        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        bodyInterface.ActivateBodies(allBodies.data(), static_cast<int>(allBodies.size()));
    }

    void PhysicsManager::DeactivateBodies()
    {
        JPH::BodyIDVector allBodies;
        s_PhysicsSystem->GetBodies(allBodies);

        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        bodyInterface.DeactivateBodies(allBodies.data(), static_cast<int>(allBodies.size()));
    }

    uint32_t PhysicsManager::CreateBody(const JPH::BodyCreationSettings& settings) {
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        JPH::BodyID bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        //s_BodyIDs.push_back(bodyID);
		//s_BodyIndexMap[bodyID.GetIndexAndSequenceNumber()] = s_BodyIDs.size() - 1;
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
        //printf("PhysicsManager: Set transform for body ID %d to position (%f, %f, %f) and rotation (%f, %f, %f)\n",
		//	index, position.x, position.y, position.z, rotation.x, rotation.y, rotation.z);
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

        //auto it = s_BodyIndexMap.find(bodyid);
        //if (it == s_BodyIndexMap.end())
        //    return; // not found

        //size_t index = it->second;
        //size_t lastIndex = s_BodyIDs.size() - 1;

        //// If it's not the last element, swap with the last
        //if (index != lastIndex) {
        //    JPH::BodyID lastID = s_BodyIDs[lastIndex];
        //    s_BodyIDs[index] = lastID;

        //    // Update the map entry for the moved element
        //    s_BodyIndexMap[lastID.GetIndexAndSequenceNumber()] = index;
        //}

        //// Remove last element
        //s_BodyIDs.pop_back();

        //// Remove from map
        //s_BodyIndexMap.erase(it);

		printf("PhysicsManager: Set motion type for body ID %d to %d\n", bodyid, static_cast<int>(motionType));
    }

    JPH::EMotionType PhysicsManager::GetMotionType(uint32_t bodyid)
    {
        if (!s_PhysicsSystem) 
            return JPH::EMotionType::Static;

        JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), JPH::BodyID(bodyid));
        if (lock.Succeeded()) 
        {
            return lock.GetBody().GetMotionType();
        }
        return JPH::EMotionType::Static;
    }

    JPH::PhysicsSystem* PhysicsManager::GetPhysicsSystem() {
        return s_PhysicsSystem.get();
    }

    uint32_t PhysicsManager::CreateBoxBody(const Math::Vec3& pos, const Math::Vec3& rot, const Math::Vec3& size, JPH::EMotionType motionType)
    {
        if (!s_PhysicsSystem)
            return 0;

        // Create box shape
        JPH::Vec3 halfExtents(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);

        JPH::BoxShapeSettings boxSettings(halfExtents);
        JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();

        if (shapeResult.HasError())
        {
            printf("PhysicsManager: Error creating box shape: %s\n", shapeResult.GetError().c_str());
            return 0;
        }

        JPH::RefConst<JPH::Shape> boxShape = shapeResult.Get();

        // Determine appropriate layer
        JPH::ObjectLayer layer = (motionType == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;

        // Create body
        JPH::BodyCreationSettings bodySettings(
            boxShape,
            JPH::RVec3(pos.x, pos.y, pos.z),
            JPH::Quat::sEulerAngles({
                JPH::DegreesToRadians(rot.x),
                JPH::DegreesToRadians(rot.y),
                JPH::DegreesToRadians(rot.z)
                }),
            motionType,
            layer
        );

        printf("CreateBoxBody successful\n");
        uint32_t result = CreateBody(bodySettings);
        return result;
    }

    void PhysicsManager::UpdateBoxSize(uint32_t bodyID, const Math::Vec3& newSize)
    {
        if (!s_PhysicsSystem)
            return;

        JPH::BodyID id(bodyID);
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();

        // Create new shape with updated size
        JPH::Vec3 halfExtents(newSize.x * 0.5f, newSize.y * 0.5f, newSize.z * 0.5f);
        JPH::BoxShapeSettings boxSettings(halfExtents);
        JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();

        if (shapeResult.HasError())
        {
            printf("PhysicsManager: Error creating box shape: %s\n", shapeResult.GetError().c_str());
            return;
        }


        JPH::RefConst<JPH::Shape> newShape = shapeResult.Get();
        // Update the body's shape
        bodyInterface.SetShape(id, newShape, true, JPH::EActivation::DontActivate);

        printf("PhysicsManager::UpdateBoxSize called\n");
    }

    uint32_t PhysicsManager::CreateSphereBody(const Math::Vec3& pos, const Math::Vec3& rot,
        float radius, JPH::EMotionType motionType) {
        if (!s_PhysicsSystem) return 0;

        printf("Creating sphere with radius: %.2f\n", radius);

        // Create sphere shape
        JPH::SphereShapeSettings sphereSettings(radius);
        JPH::ShapeSettings::ShapeResult shapeResult = sphereSettings.Create();

        if (shapeResult.HasError()) {
            printf("PhysicsManager: Error creating sphere shape: %s\n", shapeResult.GetError().c_str());
            return 0;
        }

        JPH::RefConst<JPH::Shape> sphereShape = shapeResult.Get();

        // Determine appropriate layer
        JPH::ObjectLayer layer = (motionType == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;

        // Create body
        JPH::BodyCreationSettings bodySettings(
            sphereShape,
            JPH::RVec3(pos.x, pos.y, pos.z),
            JPH::Quat::sEulerAngles({
                JPH::DegreesToRadians(rot.x),
                JPH::DegreesToRadians(rot.y),
                JPH::DegreesToRadians(rot.z)
                }),
            motionType,
            layer
        );

        printf("CreateSphereBody successful\n");
        return CreateBody(bodySettings);
    }


    //void PhysicsManager::UpdateSphereRadius(uint32_t bodyID, float newRadius)
    //{
    //}

    uint32_t PhysicsManager::CreateCapsuleBody(const Math::Vec3& pos, const Math::Vec3& rot,
        float halfHeight, float radius, JPH::EMotionType motionType) 
    {
        if (!s_PhysicsSystem) return 0;

        printf("Creating capsule with halfHeight: %.2f, radius: %.2f\n", halfHeight, radius);

        // Create capsule shape
        JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
        JPH::ShapeSettings::ShapeResult shapeResult = capsuleSettings.Create();

        if (shapeResult.HasError()) {
            printf("PhysicsManager: Error creating capsule shape: %s\n", shapeResult.GetError().c_str());
            return 0;
        }

        JPH::RefConst<JPH::Shape> capsuleShape = shapeResult.Get();

        // Determine appropriate layer
        JPH::ObjectLayer layer = (motionType == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;

        // Create body
        JPH::BodyCreationSettings bodySettings(
            capsuleShape,
            JPH::RVec3(pos.x, pos.y, pos.z),
            JPH::Quat::sEulerAngles({
                JPH::DegreesToRadians(rot.x),
                JPH::DegreesToRadians(rot.y),
                JPH::DegreesToRadians(rot.z)
                }),
            motionType,
            layer
        );

        printf("CreateCapsuleBody successful\n");
        return CreateBody(bodySettings);
    }

    void PhysicsManager::RegisterEntityBody(Entity entity, uint32_t bodyID)
    {
        s_EntityToBodyMap[entity] = bodyID;
    }

    void PhysicsManager::UnregisterEntityBody(Entity entity)
    {
        s_EntityToBodyMap.erase(entity);
    }

    uint32_t PhysicsManager::GetEntityBodyId(Entity entity)
    {
        auto it = s_EntityToBodyMap.find(entity);

        // recap wtf this do
        return (it != s_EntityToBodyMap.end()) ? it->second : 0;
    }

   
    bool PhysicsManager::EntityHasPhysicsBody(Entity entity)
    {
        return s_EntityToBodyMap.find(entity) != s_EntityToBodyMap.end();
    }
    void PhysicsManager::TestPhysicsSetup()
    {
        printf("=== PHYSICS TEST SETUP ===\n");

        // Create ground plane
        Math::Vec3 groundSize(10.0f, 1.0f, 10.0f);
        uint32_t groundBody = CreateBoxBody(
            Math::Vec3(0, -3, 0),
            Math::Vec3(0, 0, 0),
            groundSize,
            JPH::EMotionType::Static
        );
        printf("Created ground body: %u\n", groundBody);

        // Create falling box  
        Math::Vec3 boxSize(1.0f, 1.0f, 1.0f);
        uint32_t boxBody = CreateBoxBody(
            Math::Vec3(0, 5, 0),
            Math::Vec3(0, 0, 0),
            boxSize,
            JPH::EMotionType::Dynamic
        );
        printf("Created falling box body: %u\n", boxBody);

        // Activate all bodies to start simulation
        ActivateBodies();

        printf("Physics test setup complete! Box should fall onto ground.\n");
        printf("=== PHYSICS TEST SETUP COMPLETE ===\n");
    }

}