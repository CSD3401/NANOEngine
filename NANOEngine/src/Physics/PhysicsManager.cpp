#include "PhysicsManager.hpp"
#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
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
        RenderAllBodyShapes();
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

        JPH::Vec3 halfExtents(size.x * 0.5f, size.y * 0.5f, size.z * 0.5f);

        JPH::BoxShapeSettings boxSettings(halfExtents);
        JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();

        if (shapeResult.HasError())
        {
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

        if (shapeResult.HasError())
        {
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

    bool PhysicsManager::EntityHasPhysicsBody(Entity entity)
    {
        return s_EntityToBodyMap.find(entity) != s_EntityToBodyMap.end();
    }

    void PhysicsManager::TestPhysicsSetup()
    {
        printf("=== PHYSICS TEST SETUP ===\n");

        Math::Vec3 groundSize(10.0f, 1.0f, 10.0f);
        uint32_t groundBody = CreateBoxBody(
            Math::Vec3(0, -3, 0),
            Math::Vec3(0, 0, 0),
            groundSize,
            JPH::EMotionType::Static
        );
        printf("Created ground body: %u\n", groundBody);

        Math::Vec3 boxSize(1.0f, 1.0f, 1.0f);
        uint32_t boxBody = CreateBoxBody(
            Math::Vec3(0, 5, 0),
            Math::Vec3(0, 0, 0),
            boxSize,
            JPH::EMotionType::Dynamic
        );
        printf("Created falling box body: %u\n", boxBody);

        ActivateBodies();

        printf("Physics test setup complete! Box should fall onto ground.\n");
        printf("=== PHYSICS TEST SETUP COMPLETE ===\n");
    }
}