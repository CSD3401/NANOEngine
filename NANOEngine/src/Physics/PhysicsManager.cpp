#include "PhysicsManager.hpp"
#include <Jolt/RegisterTypes.h>
#include "JoltDebugRenderer.hpp"

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
        s_PhysicsSystem->Update(dt, 1, s_TempAllocator.get(), s_JobSystem.get());
        
        // Get the body interface and lock interface

        //JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        //JPH::BodyLockInterface& lockInterface = s_PhysicsSystem->GetBodyLockInterface();

        printf("Total bodies in system: %u\n", s_PhysicsSystem->GetNumBodies());
        printf("Our tracked bodies: %zu\n", s_BodyIDs.size());

        // JPH Draw Settings for debug physics line 
        JPH::BodyManager::DrawSettings drawSettings;
		drawSettings.mDrawShape = true;
        //drawSettings.mDrawBoundingBox = false;
        //drawSettings.mDrawWorldTransform = true;
        //drawSettings.mDrawSleepStats = false;
        //drawSettings.mDrawMassAndInertia = false;
        s_PhysicsSystem->DrawBodies(drawSettings, &g_joltDebugRenderer);
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

    uint32_t PhysicsManager::CreateBody(const JPH::BodyCreationSettings& settings) 
    {
        printf("CreateBody - called");
        JPH::BodyInterface& bodyInterface = s_PhysicsSystem->GetBodyInterface();
        JPH::BodyID bodyID = bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
        
        if (bodyID.IsInvalid())
        {
            printf("CreateBody - Invalid ID\n");
            return 0;
        }

        // Verify the body exists in the system
        JPH::BodyLockRead lock(s_PhysicsSystem->GetBodyLockInterface(), bodyID);
        if (!lock.Succeeded()) 
        {
            printf("ERROR: Created body cannot be locked!\n");
            return 0;
        }

        //const JPH::Body &body = lock.GetBody();
        //printf("Body locked successfully, shape: %p\n", body.GetShape().GetPtr())


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
}