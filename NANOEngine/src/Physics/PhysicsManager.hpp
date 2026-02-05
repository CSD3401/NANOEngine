#pragma once

#include <memory>
#include <array>
#include <unordered_map>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include "Core/Layers.hpp"
#include "ForceMode.hpp"
#include "ContactDefs.hpp"
#include <unordered_set>
#include <mutex>

namespace NE::Core {
    class LUIDRegistry;
}

namespace NE::ECS {
    class ComponentManager;
}

namespace NE::ECS::Component {
    struct Collider;
    struct Transform;
    struct Rigidbody;
	struct CharacterController;
	struct Renderer;
}

namespace NE::Math {
    struct Vec3;
}

namespace JPH {
    class Factory;
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemSingleThreaded;
    class BodyID;
    class CharacterVirtual;
}

namespace NE::Physics {
    class ObjectLayerPairFilterImpl;
    class BroadPhaseLayerInterfaceImpl;
    class ObjectVsBroadPhaseLayerFilterImpl;

    struct Ray;
    struct RaycastHit;

    class JoltDebugRenderer;

	class ContactListenerImpl;

    class PhysicsManager {
        struct CharacterRuntime {
            JPH::Ref<JPH::CharacterVirtual> controller;
            JPH::Vec3 velocity = JPH::Vec3::sZero();
            JPH::Vec3 pendingDelta = JPH::Vec3::sZero();
            bool hasPendingDelta = false;

            uint32_t entity = 0;
            uint64_t luid = 0;
            uint8_t layerID = 0;
        };
    public:
        static PhysicsManager& GetInstance();

        void Init();
        void Update(double dt);
        void Shutdown();


        void OnPlay();
        void OnStop();

        void SetManagers(ECS::ComponentManager* cm, Core::LUIDRegistry* lg);

        uint64_t ComputeShapeSignature(uint32_t entity, const ECS::Component::Collider& col);
        void CreateOrUpdateShape(uint32_t entity, uint64_t entityLUID, const ECS::Component::Collider& col);
        void RemoveShape(const uint64_t entityLUID);

        // Character Controller
        void CreateCharacterController(uint32_t entity, uint64_t entityLUID, 
            const ECS::Component::Transform& t, const ECS::Component::CharacterController& cc, 
            uint8_t layerID);
        void UpdateCharacters(float dt);

		bool CharacterIsGrounded(uint64_t entityLUID) const;
		void CharacterMove(uint64_t entityLUID, const Math::Vec3& displacement);
		void CharacterRotateYaw(uint64_t entityLUID, float yawDegrees);
		Math::Vec3 CharacterGetVelocity(uint64_t entityLUID) const;
		Math::Vec3 CharacterGetGroundNormal(uint64_t entityLUID) const;

        void CreateBody(uint32_t entity, uint64_t entityLUID, const ECS::Component::Transform& t, const ECS::Component::Rigidbody& rb, const ECS::Component::Collider& col, uint8_t layerID);
        void CreateBody(uint32_t entity, uint64_t entityLUID, const ECS::Component::Transform& t, const ECS::Component::Collider& col, uint8_t layerID);
        void DestroyBody(uint64_t entityLUID);

        void RemoveContactsInvolving(uint64_t luid);

        void SyncTransformToBodies(uint64_t entityLUID, ECS::Component::Transform& t) const;
		void SyncBodiesToTransform(uint64_t entityLUID, const ECS::Component::Transform& t);
		void SyncTransformToCharacters(uint64_t entityLUID, ECS::Component::Transform& t) const;
		void SyncCharactersToTransform(uint64_t entityLUID, const ECS::Component::Transform& t);

        void DrawShapeGizmo(const uint64_t entityLUID, const ECS::Component::Transform& t, const ECS::Component::Collider& col);

        bool Raycast(Math::Vec3 origin, Math::Vec3 direction, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);
        bool Raycast(Ray ray, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);
		bool SphereCast(Math::Vec3 origin, float radius, Math::Vec3 direction, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);
        bool SphereCast(Ray ray, float radius, RaycastHit& outHitInfo, float maxDistance, uint32_t layerMask);

        void AddForce(uint64_t entityLUID, Math::Vec3 force, ForceMode forceMode = ForceMode::Force);

        Math::Vec3 GetLinearVelocity(uint64_t entityLUID) const;
        void SetLinearVelocity(uint64_t entityLUID, const Math::Vec3& velocity);
        Math::Vec3 GetAngularVelocity(uint64_t entityLUID) const;
        void SetAngularVelocity(uint64_t entityLUID, const Math::Vec3& angularVelocity);

        bool CookMeshCollider(const std::vector<Math::Vec3>& vertices,
            const std::vector<uint32_t>& indices, std::vector<uint8_t>& outBlob);

		uint64_t BodyToLuid(JPH::BodyID bodyID) const;
        void PushRawContactEvent(const RawContactEvent& e);
        void FlushContactEventsAndDispatch();

        void DispatchEnter(const ContactKey& k);
        void DispatchExit(const ContactKey& k);
        void DispatchStay(const ContactKey& k);
    private:
        struct StoredShape {
            JPH::ShapeRefC shape;
            uint64_t       signature = 0;
        };

        ECS::ComponentManager* m_componentManager = nullptr;
        Core::LUIDRegistry* m_luidRegistry = nullptr;

        std::unique_ptr<JPH::Factory> m_factory;
        std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
        std::unique_ptr<JPH::TempAllocatorImpl> m_tempAllocator;
        std::unique_ptr<JPH::JobSystemSingleThreaded> m_jobSystem;

        std::array<Core::LayerMask, Core::MAX_LAYERS> m_collisionMatrix{};
        std::unique_ptr<ObjectLayerPairFilterImpl> m_objectLayerPairFilter;

        std::unique_ptr<BroadPhaseLayerInterfaceImpl> m_bpLayerInterface;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_objectVsBpFilter;

        std::unique_ptr<JoltDebugRenderer> m_debugRenderer;

        std::unordered_map<uint64_t, StoredShape> m_shapes;
        std::unordered_map<uint64_t, JPH::BodyID> m_bodies;
        std::unordered_map<JPH::BodyID, uint64_t> m_bodyToLuid;

        std::unordered_map<uint64_t, CharacterRuntime> m_characters;

        // to move to settings
        float m_fixedDt = 1.0f / 60.0f;
        int   m_collisionSteps = 1;
        float m_accumulator = 0.0f;
        float m_maxFrameTime = 0.25f;
        float m_alpha = 0.0f; // interpolation alpha for rendering

        std::mutex m_contactEventMutex;
        std::vector<RawContactEvent> m_contactEventsWrite;
        std::vector<RawContactEvent> m_contactEventsRead;

        std::unordered_set<ContactKey, ContactKeyHash> m_prevContacts;
        std::unordered_set<ContactKey, ContactKeyHash> m_currContacts;

        std::unique_ptr<ContactListenerImpl> m_contactListener;
    };

}
