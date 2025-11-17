#include "PhysicsManager.hpp"
#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

// Raycasting includes
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Geometry/IndexedTriangle.h>
// Constraint includes for rotation locking
#include <Jolt/Physics/Constraints/SixDOFConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>

// debug
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
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

    std::unique_ptr<JPH::Factory> PhysicsManager::s_Factory;
    std::unique_ptr<JPH::PhysicsSystem> PhysicsManager::s_PhysicsSystem;
    std::unique_ptr<JPH::TempAllocatorImpl> PhysicsManager::s_TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> PhysicsManager::s_JobSystem;
    std::unique_ptr<PhysicsContactListener> PhysicsManager::s_ContactListener;
    std::unordered_map<NE::ECS::Entity, uint32_t> PhysicsManager::s_EntityToBodyMap;

	static BPLayerInterfaceImpl s_BPLayerInterface;
	static ObjectVsBroadPhaseLayerFilterImpl s_ObjectVsBroadPhaseLayerFilter;
	static ObjectLayerPairFilterImpl s_ObjectLayerPairFilter;

	std::unordered_map<uint32_t, JPH::RefConst<JPH::Shape>> PhysicsManager::s_shapeMap;

	static JoltDebugRenderer g_joltDebugRenderer;

//     std::vector<JPH::BodyID> g_TestBodyIDs; // MUST store IDs to keep bodies alive!

// Commented out, affecting colliding stuff, bodies not properly created - RF
#pragma region Amanda Test 
// #pragma region test Jolt Physics Debug Draw - INIT (call once at startup)
//     void TestJoltPhysicsDebugDraw_Init()
//     {
//         std::cout << "[JoltDebugDrawTest] Initializing test bodies..." << std::endl;

//         JPH::PhysicsSystem* ps = NE::Physics::PhysicsManager::GetPhysicsSystem();
//         if (!ps) {
//             std::cerr << "ERROR: Physics system not available!" << std::endl;
//             return;
//         }

//         JPH::BodyInterface& bi = ps->GetBodyInterface();

//         // Clear any existing test bodies
//         g_TestBodyIDs.clear();

//         // -- Floor (static) - Use layer 0 for static objects
//         {
//             JPH::RefConst<JPH::Shape> floor = new JPH::BoxShape(JPH::Vec3(10.0f, 0.2f, 10.0f));
//             JPH::BodyCreationSettings s(floor, JPH::RVec3(0, -0.2f, 0),
//                 JPH::Quat::sIdentity(),
//                 JPH::EMotionType::Static,
//                 Layers::NON_MOVING);
//             JPH::BodyID id = bi.CreateAndAddBody(s, JPH::EActivation::DontActivate);
//             if (!id.IsInvalid()) {
//                 g_TestBodyIDs.push_back(id); // STORE IT!
//                 std::cout << "  - Created floor (static) - ID: " << id.GetIndex() << std::endl;
//             }
//         }

//         // -- Box (dynamic)
//         {
//             JPH::RefConst<JPH::Shape> box = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
//             JPH::BodyCreationSettings s(box, JPH::RVec3(-2.0, 3.5, 0.0),
//                 JPH::Quat::sIdentity(),
//                 JPH::EMotionType::Dynamic,
//                 Layers::MOVING);
//             JPH::BodyID id = bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//             if (!id.IsInvalid()) {
//                 g_TestBodyIDs.push_back(id); // STORE IT!
//                 std::cout << "  - Created box (dynamic) at (-2, 3.5, 0) - ID: " << id.GetIndex() << std::endl;
//             }
//         }

//         // -- Sphere (dynamic)
//         {
//             JPH::RefConst<JPH::Shape> sph = new JPH::SphereShape(0.5f);
//             JPH::BodyCreationSettings s(sph, JPH::RVec3(0.0, 3.5, 0.0),
//                 JPH::Quat::sIdentity(),
//                 JPH::EMotionType::Dynamic,
//                 Layers::MOVING);
//             JPH::BodyID id = bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//             if (!id.IsInvalid()) {
//                 g_TestBodyIDs.push_back(id); // STORE IT!
//                 std::cout << "  - Created sphere (dynamic) at (0, 3.5, 0) - ID: " << id.GetIndex() << std::endl;
//             }
//         }

//         // -- Capsule (dynamic)
//         {
//             float halfHeight = 0.5f;
//             float radius = 0.25f;
//             JPH::RefConst<JPH::Shape> cap = new JPH::CapsuleShape(halfHeight, radius);
//             JPH::BodyCreationSettings s(cap, JPH::RVec3(2.0, 3.75, 0.0),
//                 JPH::Quat::sIdentity(),
//                 JPH::EMotionType::Dynamic,
//                 Layers::MOVING);
//             JPH::BodyID id = bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//             if (!id.IsInvalid()) {
//                 g_TestBodyIDs.push_back(id); // STORE IT!
//                 std::cout << "  - Created capsule (dynamic) at (2, 3.75, 0) - ID: " << id.GetIndex() << std::endl;
//             }
//         }

//         // -- Cylinder (dynamic)
//         {
//             float halfHeight = 0.5f;
//             float radius = 0.3f;
//             JPH::RefConst<JPH::Shape> cyl = new JPH::CylinderShape(halfHeight, radius);
//             JPH::BodyCreationSettings s(cyl, JPH::RVec3(4.0, 3.5, 0.0),
//                 JPH::Quat::sIdentity(),
//                 JPH::EMotionType::Dynamic,
//                 Layers::MOVING);
//             JPH::BodyID id = bi.CreateAndAddBody(s, JPH::EActivation::Activate);
//             if (!id.IsInvalid()) {
//                 g_TestBodyIDs.push_back(id); // STORE IT!
//                 std::cout << "  - Created cylinder (dynamic) at (4, 3.5, 0) - ID: " << id.GetIndex() << std::endl;
//             }
//         }

//         std::cout << "  - Total bodies created and stored: " << g_TestBodyIDs.size() << std::endl;
//     }
// #pragma endregion

// #pragma region test Jolt Physics Debug Draw - RENDER (call every frame)
//     void TestJoltPhysicsDebugDraw_Render()
//     {
//         using DR = JPH::DebugRenderer;

//         g_joltDebugRenderer.BeginFrame();

//         // Test 1 - primitive shapes
//         {
//             // sphere solid
//             g_joltDebugRenderer.DrawSphere(JPH::RVec3(0.0, 1.0, 0.0), 0.5f, JPH::Color::sGreen, DR::ECastShadow::Off, DR::EDrawMode::Solid);

//             // sphere wireframe
//             JPH::RMat44 T = JPH::RMat44::sTranslation(JPH::RVec3(1.25, 1.0, 0.0));
//             JPH::RMat44 S = JPH::RMat44::sScale(JPH::Vec3(0.5f, 0.5f, 0.5f));
//             g_joltDebugRenderer.DrawUnitSphere(T * S, JPH::Color::sRed, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);

//             // capsule solid
//             g_joltDebugRenderer.DrawCapsule(JPH::RMat44::sTranslation(JPH::RVec3(3.0, 1.0, 0.0)), 0.5f, 0.25f, JPH::Color::sYellow, DR::ECastShadow::Off, DR::EDrawMode::Solid);

//             // capsule wireframe
//             g_joltDebugRenderer.DrawCapsule(JPH::RMat44::sTranslation(JPH::RVec3(4.0, 1.0, 0.0)), 0.5f, 0.25f, JPH::Color::sOrange, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);

//             // cylinder solid
//             g_joltDebugRenderer.DrawCylinder(JPH::RMat44::sTranslation(JPH::RVec3(5.5, 1.0, 0.0)), 0.5f, 0.3f, JPH::Color::sCyan, DR::ECastShadow::Off, DR::EDrawMode::Solid);

//             // cylinder wireframe
//             g_joltDebugRenderer.DrawCylinder(JPH::RMat44::sTranslation(JPH::RVec3(6.5, 1.0, 0.0)), 0.5f, 0.3f, JPH::Color::sBlue, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);
//         }

//         // Test 2 - custom triangle (created each frame for simplicity)
//         {
//             DR::Vertex verts[3] = {};
//             verts[0].mPosition = JPH::Float3(0.0f, 0.0f, 0.0f);
//             verts[1].mPosition = JPH::Float3(1.0f, 0.0f, 0.0f);
//             verts[2].mPosition = JPH::Float3(0.0f, 1.0f, 0.0f);

//             const JPH::uint32 indices[3] = { 0, 1, 2 };
//             DR::Batch batch = g_joltDebugRenderer.CreateTriangleBatch(verts, 3, indices, 3);
//             if (batch) {
//                 const JPH::AABox bounds = DR::sCalculateBounds(verts, 3);
//                 DR::GeometryRef geom = new DR::Geometry(batch, bounds);

//                 // solid triangle
//                 JPH::RMat44 Msolid = JPH::RMat44::sTranslation(JPH::RVec3(0.0, 2.0, 0.0));
//                 g_joltDebugRenderer.DrawGeometry(Msolid, geom->mBounds.Transformed(Msolid), 1.0f, JPH::Color::sWhite, geom, DR::ECullMode::Off, DR::ECastShadow::Off, DR::EDrawMode::Solid);

//                 // wireframe triangle
//                 JPH::RMat44 Mwire = JPH::RMat44::sTranslation(JPH::RVec3(1.25, 2.0, 0.0));
//                 g_joltDebugRenderer.DrawGeometry(Mwire, geom->mBounds.Transformed(Mwire), 1.0f, JPH::Color::sRed, geom, DR::ECullMode::Off, DR::ECastShadow::Off, DR::EDrawMode::Wireframe);
//             }
//         }

//         // Test 3 - draw all physics bodies
//         {
//             JPH::PhysicsSystem* ps = NE::Physics::PhysicsManager::GetPhysicsSystem();
//             if (ps) {
//                 JPH::BodyManager::DrawSettings drawSettings;
//                 drawSettings.mDrawShape = true;                       // Draw solid shapes
//                 drawSettings.mDrawShapeWireframe = false;             // Set true to overlay wireframe on shapes
//                 drawSettings.mDrawBoundingBox = true;                 // Set true to see axis-aligned bounding boxes (AABBs)
//                 //drawSettings.mDrawShapeColor = JPH::BodyManager::EShapeColor::MotionTypeColor;  // Color: Static=gray, Dynamic=green (affected by forces), Kinematic=yellow (not affected by forces)
//                 //drawSettings.mDrawCenterOfMassTransform = false;    // Draw tiny RGB axes at center of mass (can be hard to see as they are very small, need to scale down entity's size)
//                 //drawSettings.mDrawWorldTransform = false;           // need to update body position in physics system to work
//                 //drawSettings.mDrawVelocity = false;                 // Draw velocity vectors as arrows (only for dynamic bodies)
//                 //drawSettings.mDrawGetSupportFunction = false;       // Advanced: draw support point in specified direction (not sure how this works)
//                 //drawSettings.mDrawGetSupportingFace = false;        // Advanced: draw supporting face for collision detection (not sure how this works)

//                 ps->DrawBodies(drawSettings, &g_joltDebugRenderer);
//             }
//         }

//         g_joltDebugRenderer.EndFrame();
//     }
// #pragma endregion

// #pragma region test Jolt Physics Debug Draw - SHUTDOWN
//     void TestJoltPhysicsDebugDraw_Shutdown()
//     {
//         JPH::PhysicsSystem* ps = NE::Physics::PhysicsManager::GetPhysicsSystem();
//         if (!ps) return;

//         JPH::BodyInterface& bi = ps->GetBodyInterface();

//         std::cout << "[JoltDebugDrawTest] Cleaning up " << g_TestBodyIDs.size() << " test bodies..." << std::endl;

//         for (JPH::BodyID id : g_TestBodyIDs) {
//             bi.RemoveBody(id);
//             bi.DestroyBody(id);
//         }

//         g_TestBodyIDs.clear();
//         std::cout << "  - Cleanup complete" << std::endl;
//     }
// #pragma endregion
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
        const unsigned int cNumBodyMutexes = 0;
        const unsigned int cMaxBodyPairs = 1024;
        const unsigned int cMaxContactConstraints = 1024;
        s_PhysicsSystem->Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
            s_BPLayerInterface, s_ObjectVsBroadPhaseLayerFilter, s_ObjectLayerPairFilter);

        s_ContactListener = std::make_unique<PhysicsContactListener>();
        s_PhysicsSystem->SetContactListener(s_ContactListener.get());
    }

	void PhysicsManager::Update(float dt) {
		if (!s_PhysicsSystem)
			return;

        static bool firstFrame = true;
        if (firstFrame)
        {
            firstFrame = false;
            return;
        }

        // Keep bodies awake so collision callbacks keep firing
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        JPH::BodyIDVector allBodies;
        s_PhysicsSystem->GetBodies(allBodies);
        bodyInterface.ActivateBodies(allBodies.data(), static_cast<int>(allBodies.size()));

        // Run physics simulation
        s_PhysicsSystem->Update(dt, /*numSubSteps=*/2, s_TempAllocator.get(), s_JobSystem.get());

        // Check which collisions ended this frame
        if (s_ContactListener)
        {
            s_ContactListener->UpdateCollisionStates();
        }

        // Render debug shapes
        //RenderAllBodyShapes();
    }

    void PhysicsManager::RenderAllBodyShapes()
    {
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        (void)bodyInterface;

        JPH::BodyIDVector bodyIDs;
        s_PhysicsSystem->GetBodies(bodyIDs);

        for (JPH::BodyID bodyID : bodyIDs)
        {
            JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), bodyID);
            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();
                RenderBodyShape(body);
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

        shape->Draw(
            &g_joltDebugRenderer,
            transform,
            JPH::Vec3::sReplicate(1.0f),
            color,
            false,
            true
        );
    }

    void PhysicsManager::RegisterCollisionEnterCallback(PhysicsContactListener::CollisionCallback callback)
    {
        if (s_ContactListener)
        {
            s_ContactListener->RegisterCollisionEnterCallback(callback);
        }
    }

    void PhysicsManager::RegisterCollisionStayCallback(PhysicsContactListener::CollisionCallback callback)
    {
        if (s_ContactListener)
        {
            s_ContactListener->RegisterCollisionStayCallback(callback);
        }
    }

    void PhysicsManager::RegisterCollisionExitCallback(PhysicsContactListener::CollisionCallback callback)
    {
        if (s_ContactListener)
        {
            s_ContactListener->RegisterCollisionExitCallback(callback);
        }
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
        return bodyID.GetIndexAndSequenceNumber();
    }

    void PhysicsManager::DestroyBody(uint32_t index) {
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

        bodyInterface.SetPositionAndRotation(id, joltPos, joltRot, JPH::EActivation::DontActivate);
    }

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
    }

    JPH::EMotionType PhysicsManager::GetMotionType(uint32_t bodyid)
    {
        if (!s_PhysicsSystem)
            return JPH::EMotionType::Static;

        JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), JPH::BodyID(bodyid));
        if (lock.Succeeded()) {
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

        JPH::Vec3 halfExtents(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);

        JPH::BoxShapeSettings boxSettings(halfExtents);
        JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();

        if (shapeResult.HasError()) {
            printf("PhysicsManager: Error creating box shape: %s\n", shapeResult.GetError().c_str());
            return 0;
        }

        JPH::RefConst<JPH::Shape> boxShape = shapeResult.Get();

        JPH::ObjectLayer layer = (motionType == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;

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

        bodySettings.mAllowDynamicOrKinematic = true;

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

        JPH::Vec3 halfExtents(newSize.x * 0.5f, newSize.y * 0.5f, newSize.z * 0.5f);
        JPH::BoxShapeSettings boxSettings(halfExtents);
        JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();

        if (shapeResult.HasError()) {
            printf("PhysicsManager: Error creating box shape: %s\n", shapeResult.GetError().c_str());
            return;
        }

        JPH::RefConst<JPH::Shape> newShape = shapeResult.Get();
        bodyInterface.SetShape(id, newShape, true, JPH::EActivation::DontActivate);

        printf("PhysicsManager::UpdateBoxSize called\n");
    }

    uint32_t PhysicsManager::CreateSphereBody(const Math::Vec3& pos, const Math::Vec3& rot,
        float radius, JPH::EMotionType motionType) {
        if (!s_PhysicsSystem) return 0;

        printf("Creating sphere with radius: %.2f\n", radius);

        JPH::SphereShapeSettings sphereSettings(radius);
        JPH::ShapeSettings::ShapeResult shapeResult = sphereSettings.Create();

        if (shapeResult.HasError()) {
            printf("PhysicsManager: Error creating sphere shape: %s\n", shapeResult.GetError().c_str());
            return 0;
        }

        JPH::RefConst<JPH::Shape> sphereShape = shapeResult.Get();

        JPH::ObjectLayer layer = (motionType == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;

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

        bodySettings.mAllowDynamicOrKinematic = true;

        printf("CreateSphereBody successful\n");
        return CreateBody(bodySettings);
    }

	uint32_t PhysicsManager::CreateMeshShape(std::string meshID, const std::vector<Math::Vec3>& vertices, const std::vector<uint32_t>& indices) {
        if (!s_PhysicsSystem)
            return 0;

        if (vertices.empty() || indices.size() < 3 || indices.size() % 3 != 0) {
            printf("PhysicsManager::CreateMeshShape - invalid mesh data (verts = %zu, indices = %zu)\n",
                vertices.size(), indices.size());
            return 0;
        }

        uint32_t key = std::hash<std::string>{}(meshID);
        JPH::RefConst<JPH::Shape> meshShape;

        if (auto it = s_shapeMap.find(key); it != s_shapeMap.end()) {
            meshShape = it->second;
        } else {
            JPH::VertexList joltVerts;
            joltVerts.reserve(vertices.size());
            for (const auto& v : vertices) {
                // Jolt VertexList = Array<Vec3>
                joltVerts.emplace_back(v.x, v.y, v.z);
            }

            JPH::IndexedTriangleList tris;
            tris.reserve(indices.size() / 3);

            // Each IndexedTriangle is (i0, i1, i2, materialIndex)
            for (size_t i = 0; i < indices.size(); i += 3) {
                uint32_t i0 = indices[i + 0];
                uint32_t i1 = indices[i + 1];
                uint32_t i2 = indices[i + 2];

                tris.emplace_back(
                    i0,
                    i1,
                    i2,
                    0u               // material index: all triangles use material 0 for now
                );
            }

            JPH::MeshShapeSettings meshSettings(std::move(joltVerts), std::move(tris));

            // Optional tuning:
            meshSettings.mBuildQuality = JPH::MeshShapeSettings::EBuildQuality::FavorRuntimePerformance;
            meshSettings.mPerTriangleUserData = false; // set true per-tri user data is needed

            // Build the MeshShape (BVH etc.)
            JPH::ShapeSettings::ShapeResult shapeResult = meshSettings.Create();
            if (shapeResult.HasError()) {
                printf("PhysicsManager::CreateMeshShape - error: %s\n", shapeResult.GetError().c_str());
                return 0;
            }

            meshShape = shapeResult.Get();

            // Cache the shape for reuse (same meshID = same shape)
            s_shapeMap[key] = meshShape;
        }

        // ----- 5) Create the body using the mesh shape -----
        // MeshShape::MustBeStatic() returns true this body must be STATIC.
        JPH::BodyCreationSettings bodySettings(
            meshShape,
            JPH::RVec3::sZero(),      // position - you can update later with SetTransform
            JPH::Quat::sIdentity(),   // rotation
            JPH::EMotionType::Static, // must be Static for non-convex mesh
            Layers::NON_MOVING
        );

        

        // Allow marking as kinematic later if you ever need that
        // bodySettings.mAllowDynamicOrKinematic = true;

        uint32_t bodyID = CreateBody(bodySettings);

        printf("PhysicsManager::CreateMeshShape - created mesh body %u (%zu verts, %zu tris)\n",
            bodyID, vertices.size(), indices.size() / 3);

        return bodyID;
	}

    uint32_t PhysicsManager::CreateCapsuleBody(const Math::Vec3& pos, const Math::Vec3& rot,
        float halfHeight, float radius, JPH::EMotionType motionType)
    {
        if (!s_PhysicsSystem) return 0;

        printf("Creating capsule with halfHeight: %.2f, radius: %.2f\n", halfHeight, radius);

        JPH::CapsuleShapeSettings capsuleSettings(halfHeight, radius);
        JPH::ShapeSettings::ShapeResult shapeResult = capsuleSettings.Create();

        if (shapeResult.HasError()) {
            printf("PhysicsManager: Error creating capsule shape: %s\n", shapeResult.GetError().c_str());
            return 0;
        }

        JPH::RefConst<JPH::Shape> capsuleShape = shapeResult.Get();

        JPH::ObjectLayer layer = (motionType == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;

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

        bodySettings.mAllowDynamicOrKinematic = true;

        printf("CreateCapsuleBody successful\n");
        return CreateBody(bodySettings);
    }

    void PhysicsManager::RegisterEntityBody(Entity entity, uint32_t bodyID)
    {
        s_EntityToBodyMap[entity] = bodyID;
        if (s_ContactListener)
        {
            s_ContactListener->MapBodyToEntity(JPH::BodyID(bodyID), entity);
        }
    }

    void PhysicsManager::UnregisterEntityBody(Entity entity)
    {
        auto it = s_EntityToBodyMap.find(entity);
        if (it != s_EntityToBodyMap.end())
        {
            if (s_ContactListener)
            {
                s_ContactListener->UnmapBody(JPH::BodyID(it->second));
            }
            s_EntityToBodyMap.erase(it);
        }
    }

    uint32_t PhysicsManager::GetEntityBodyId(Entity entity)
    {
        auto it = s_EntityToBodyMap.find(entity);
        return (it != s_EntityToBodyMap.end()) ? it->second : 0;
    }

	Entity PhysicsManager::GetBodyEntity(uint32_t bodyID) 
	{
		// Search through the entity-to-body map to find matching entity
		for (const auto& [entity, id] : s_EntityToBodyMap) {
			if (id == bodyID) {
				return entity;
			}
		}
		return 0; // Return invalid entity if not found
	}

    bool PhysicsManager::EntityHasPhysicsBody(Entity entity)
    {
        return s_EntityToBodyMap.find(entity) != s_EntityToBodyMap.end();
    }

	// === Velocity and Force Methods ===

	Math::Vec3 PhysicsManager::GetLinearVelocity(uint32_t bodyID) {
		JPH::BodyID id(bodyID);
		JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
		JPH::Vec3 velocity = bodyInterface.GetLinearVelocity(id);
		return Math::Vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ());
	}

	void PhysicsManager::SetLinearVelocity(uint32_t bodyID, const Math::Vec3& velocity) {
		JPH::BodyID id(bodyID);
		JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
		bodyInterface.SetLinearVelocity(id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
	}

	void PhysicsManager::AddForce(uint32_t bodyID, const Math::Vec3& force) {
		JPH::BodyID id(bodyID);
		JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
		bodyInterface.AddForce(id, JPH::Vec3(force.x, force.y, force.z));
	}

	void PhysicsManager::AddImpulse(uint32_t bodyID, const Math::Vec3& impulse) {
		JPH::BodyID id(bodyID);
		JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
		bodyInterface.AddImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
	}

	// Add this to PhysicsManager.cpp

	void PhysicsManager::SetGravityEnabled(uint32_t bodyID, bool enabled) {
		if (!s_PhysicsSystem) {
			printf("ERROR: PhysicsSystem is null in SetGravityEnabled!\n");
			return;
		}

		JPH::BodyID id(bodyID);

		// Lock the body to modify its properties
		JPH::BodyLockWrite lock(s_PhysicsSystem->GetBodyLockInterface(), id);
		if (lock.Succeeded()) {
			JPH::Body& body = lock.GetBody();

			if (body.IsDynamic()) {
				JPH::MotionProperties* motionProps = body.GetMotionProperties();

				if (enabled) {
					// Enable gravity - use default gravity factor (1.0)
					motionProps->SetGravityFactor(1.0f);
				}
				else {
					// Disable gravity - set gravity factor to 0
					motionProps->SetGravityFactor(0.0f);
				}

				printf("PhysicsManager: Set gravity for body %u to %s (factor: %.1f)\n",
					bodyID, enabled ? "ENABLED" : "DISABLED",
					motionProps->GetGravityFactor());
			}
			else {
				printf("PhysicsManager: Body %u is not dynamic, cannot set gravity\n", bodyID);
			}
		}
		else {
			printf("PhysicsManager: Failed to lock body %u for gravity change\n", bodyID);
		}
	}
	// === Rotation Locking ===

	void PhysicsManager::LockRotation(uint32_t bodyID, bool lockX, bool lockY, bool lockZ) 
	{
		if (!s_PhysicsSystem) return;

		JPH::BodyID id(bodyID);
		JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();

		// Stop any existing rotation first
		bodyInterface.SetAngularVelocity(id, JPH::Vec3::sZero());

		// Lock rotation by modifying the body's allowed degrees of freedom
		JPH::BodyLockWrite lock(s_PhysicsSystem->GetBodyLockInterface(), id);
		if (lock.Succeeded()) {
			JPH::Body& body = lock.GetBody();

			if (body.IsDynamic()) {
				JPH::MotionProperties* motionProps = body.GetMotionProperties();

				// Build the allowed DOFs bitmask
			 // Bits: 0=TransX, 1=TransY, 2=TransZ, 3=RotX, 4=RotY, 5=RotZ
				uint32_t dofBits = 0;

				// Always allow all translation
				dofBits |= (1 << 0);  // TranslationX
				dofBits |= (1 << 1);  // TranslationY
				dofBits |= (1 << 2);  // TranslationZ

				// Add rotation DOFs only if NOT locked
				if (!lockX) dofBits |= (1 << 3);  // RotationX
				if (!lockY) dofBits |= (1 << 4);  // RotationY
				if (!lockZ) dofBits |= (1 << 5);  // RotationZ

				JPH::EAllowedDOFs allowedDOFs = static_cast<JPH::EAllowedDOFs>(dofBits);

				// Get the current mass from the body
				float mass = motionProps->GetInverseMass() > 0.0f
					? 1.0f / motionProps->GetInverseMass()
					: 70.0f;

				// Get mass properties from the shape
				JPH::MassProperties massProps = body.GetShape()->GetMassProperties();
				massProps.mMass = mass;

				// Apply the new mass properties with restricted DOFs
				motionProps->SetMassProperties(allowedDOFs, massProps);
			}
		}
	}

	void PhysicsManager::ClearAllBodies() {
		if (!s_PhysicsSystem)
			return;

		JPH::BodyIDVector allBodies;
		s_PhysicsSystem->GetBodies(allBodies);

		JPH::BodyInterface& bi = s_PhysicsSystem->GetBodyInterface();

		for (JPH::BodyID id : allBodies) {
			bi.RemoveBody(id);
			bi.DestroyBody(id);
		}

		s_EntityToBodyMap.clear();
	}

	// === Raycasting Methods with Layer Filtering ===

	PhysicsManager::RaycastHit PhysicsManager::Raycast(
		const Math::Vec3& origin,
		const Math::Vec3& direction,
		float maxDistance,
		uint32_t layerMask)
	{
		RaycastHit hit;
		hit.hasHit = false;

		if (!s_PhysicsSystem) {
			return hit;
		}

		// Validate input
		if (maxDistance <= 0.0f) {
			return hit;
		}

		// Normalize direction
		JPH::Vec3 joltDir(direction.x, direction.y, direction.z);
		float dirLength = joltDir.Length();
		if (dirLength < 0.0001f) {
			return hit;  // Invalid direction
		}
		joltDir = joltDir / dirLength;  // Normalize

		// Create ray
		JPH::RRayCast ray;
		ray.mOrigin = JPH::RVec3(origin.x, origin.y, origin.z);
		ray.mDirection = joltDir * maxDistance;

		// Create object layer filter
		class ObjectLayerFilter : public JPH::ObjectLayerFilter {
		public:
			uint32_t mLayerMask;

			ObjectLayerFilter(uint32_t mask) : mLayerMask(mask) {}

			virtual bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
				// Check if this layer's bit is set in the mask
				return (mLayerMask & (1 << inLayer)) != 0;
			}
		};

		ObjectLayerFilter layerFilter(layerMask);

		// Perform raycast with object layer filtering
		JPH::RayCastResult result;
		bool hasHit = s_PhysicsSystem->GetNarrowPhaseQuery().CastRay(
			ray,
			result,
			JPH::BroadPhaseLayerFilter(),
			layerFilter,
			JPH::BodyFilter()
		);

		// Check if we hit something
		if (hasHit && !result.mBodyID.IsInvalid()) {
			hit.hasHit = true;
			hit.distance = result.mFraction * maxDistance;

			// Calculate hit point
			JPH::RVec3 hitPoint = ray.mOrigin + ray.mDirection * result.mFraction;
			hit.point = Math::Vec3(
				static_cast<float>(hitPoint.GetX()),
				static_cast<float>(hitPoint.GetY()),
				static_cast<float>(hitPoint.GetZ())
			);

			// Get surface normal
			JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), result.mBodyID);
			if (lock.Succeeded()) {
				const JPH::Body& body = lock.GetBody();
				JPH::Vec3 joltNormal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, hitPoint);
				hit.normal = Math::Vec3(joltNormal.GetX(), joltNormal.GetY(), joltNormal.GetZ());
			}

			// Store body ID and find associated entity
			hit.bodyID = result.mBodyID.GetIndexAndSequenceNumber();
			hit.entity = GetBodyEntity(hit.bodyID);
		}

		return hit;
	}

	std::vector<PhysicsManager::RaycastHit> PhysicsManager::RaycastAll(
		const Math::Vec3& origin,
		const Math::Vec3& direction,
		float maxDistance,
		uint32_t layerMask)
	{
		std::vector<RaycastHit> hits;

		if (!s_PhysicsSystem) {
			return hits;
		}

		// Validate input
		if (maxDistance <= 0.0f) {
			return hits;
		}

		// Normalize direction
		JPH::Vec3 joltDir(direction.x, direction.y, direction.z);
		float dirLength = joltDir.Length();
		if (dirLength < 0.0001f) {
			return hits;  // Invalid direction
		}
		joltDir = joltDir / dirLength;  // Normalize

		// Create ray
		JPH::RRayCast ray;
		ray.mOrigin = JPH::RVec3(origin.x, origin.y, origin.z);
		ray.mDirection = joltDir * maxDistance;

		// Settings for all hits
		JPH::RayCastSettings settings;

		// Create object layer filter
		class ObjectLayerFilter : public JPH::ObjectLayerFilter {
		public:
			uint32_t mLayerMask;

			ObjectLayerFilter(uint32_t mask) : mLayerMask(mask) {}

			virtual bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
				return (mLayerMask & (1 << inLayer)) != 0;
			}
		};

		ObjectLayerFilter layerFilter(layerMask);

		// Collector to gather all hits
		class AllHitsCollector : public JPH::CastRayCollector {
		public:
			std::vector<JPH::RayCastResult> mResults;

			virtual void AddHit(const JPH::RayCastResult& inResult) override {
				mResults.push_back(inResult);
			}
		};

		AllHitsCollector collector;

		// Perform raycast with collector and object layer filtering
		s_PhysicsSystem->GetNarrowPhaseQuery().CastRay(
			ray,
			settings,
			collector,
			JPH::BroadPhaseLayerFilter(),
			layerFilter,
			JPH::BodyFilter()
		);

		// Process all hits
		for (const auto& result : collector.mResults) {
			RaycastHit hit;
			hit.hasHit = true;
			hit.distance = result.mFraction * maxDistance;

			// Calculate hit point
			JPH::RVec3 hitPoint = ray.mOrigin + ray.mDirection * result.mFraction;
			hit.point = Math::Vec3(
				static_cast<float>(hitPoint.GetX()),
				static_cast<float>(hitPoint.GetY()),
				static_cast<float>(hitPoint.GetZ())
			);

			// Get surface normal
			JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), result.mBodyID);
			if (lock.Succeeded()) {
				const JPH::Body& body = lock.GetBody();
				JPH::Vec3 joltNormal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, hitPoint);
				hit.normal = Math::Vec3(joltNormal.GetX(), joltNormal.GetY(), joltNormal.GetZ());
			}

			// Store body ID and entity
			hit.bodyID = result.mBodyID.GetIndexAndSequenceNumber();
			hit.entity = GetBodyEntity(hit.bodyID);

			hits.push_back(hit);
		}

		// Sort hits by distance (closest first)
		std::sort(hits.begin(), hits.end(), [](const RaycastHit& a, const RaycastHit& b) {
			return a.distance < b.distance;
			});

		return hits;
	}
}