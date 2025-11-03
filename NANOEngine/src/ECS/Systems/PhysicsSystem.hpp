#pragma once
#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../../Physics/PhysicsManager.hpp"
#include "../Components/Collider.hpp"
#include "../Components/Transform.hpp"
#include "../Components/Rigidbody.hpp"

namespace NE::ECS::Systems
{
    class PhysicsSystem final : public System {
    public:
        explicit PhysicsSystem(ComponentManager* cm);

        void OnEntityAdded(Entity entity) override;
        void OnEntityRemoved(Entity entity) override;

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;

        // ========================================
        // STATIC HELPER METHODS FOR SCRIPTS
        // ========================================
        // These methods allow IScript to interact with physics
        // without directly accessing the component manager

        static void SetComponentManager(ComponentManager* cm) { s_componentManager = cm; }

        // Check if entity has physics body
        static bool HasPhysicsBody(Entity entity) {
            return NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity);
        }

        // Velocity methods
        static Math::Vec3 GetVelocity(Entity entity) {
            if (!HasPhysicsBody(entity)) return Math::Vec3(0, 0, 0);
            uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
            return NE::Physics::PhysicsManager::GetLinearVelocity(bodyID);
        }

        static void SetVelocity(Entity entity, const Math::Vec3& velocity) {
            if (!HasPhysicsBody(entity)) return;
            uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
            NE::Physics::PhysicsManager::SetLinearVelocity(bodyID, velocity);
        }

        // Force methods
        static void AddForce(Entity entity, const Math::Vec3& force) {
            if (!HasPhysicsBody(entity)) return;
            uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
            NE::Physics::PhysicsManager::AddForce(bodyID, force);
        }

        static void AddImpulse(Entity entity, const Math::Vec3& impulse) {
            if (!HasPhysicsBody(entity)) return;
            uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
            NE::Physics::PhysicsManager::AddImpulse(bodyID, impulse);
        }

        // Rotation locking
        static void LockRotation(Entity entity, bool lockX, bool lockY, bool lockZ) {
            if (!HasPhysicsBody(entity)) return;
            uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
            NE::Physics::PhysicsManager::LockRotation(bodyID, lockX, lockY, lockZ);
        }

        // Gravity control
        static void SetUseGravity(Entity entity, bool enabled) {
            if (!HasPhysicsBody(entity)) return;
            uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
            NE::Physics::PhysicsManager::SetGravityEnabled(bodyID, enabled);
        }

        static bool GetUseGravity(Entity entity) {
            if (!HasPhysicsBody(entity) || !s_componentManager) return false;
            if (!s_componentManager->HasComponent<Component::Rigidbody>(entity)) return false;

            auto& rb = s_componentManager->GetComponent<Component::Rigidbody>(entity);
            return rb.useGravity;
        }

        // Mass control
        static void SetMass(Entity entity, float mass) {
            if (!HasPhysicsBody(entity) || !s_componentManager) return;
            if (!s_componentManager->HasComponent<Component::Rigidbody>(entity)) return;

            auto& rb = s_componentManager->GetComponent<Component::Rigidbody>(entity);
            rb.mass = mass;
            // Note: You may need to add a SetMass method to PhysicsManager
        }

        static float GetMass(Entity entity) {
            if (!HasPhysicsBody(entity) || !s_componentManager) return 1.0f;
            if (!s_componentManager->HasComponent<Component::Rigidbody>(entity)) return 1.0f;

            auto& rb = s_componentManager->GetComponent<Component::Rigidbody>(entity);
            return rb.mass;
        }

        // Raycasting (pass through to PhysicsManager)
        static Physics::PhysicsManager::RaycastHit Raycast(
            const Math::Vec3& origin,
            const Math::Vec3& direction,
            float maxDistance,
            uint32_t layerMask = Physics::PhysicsManager::LAYER_ALL)
        {
            return NE::Physics::PhysicsManager::Raycast(origin, direction, maxDistance, layerMask);
        }

        static std::vector<Physics::PhysicsManager::RaycastHit> RaycastAll(
            const Math::Vec3& origin,
            const Math::Vec3& direction,
            float maxDistance,
            uint32_t layerMask = Physics::PhysicsManager::LAYER_ALL)
        {
            return NE::Physics::PhysicsManager::RaycastAll(origin, direction, maxDistance, layerMask);
        }

    private:
        ComponentManager* m_componentManager;
        static ComponentManager* s_componentManager; // Static pointer for helper methods

        // Collision callback handlers
        void HandleCollisionEnter(const NE::Physics::CollisionInfo& collision);
        void HandleCollisionStay(const NE::Physics::CollisionInfo& collision);
        void HandleCollisionExit(const NE::Physics::CollisionInfo& collision);

        // ECS to Physics: Create/update physics bodies for new/changed colliders
        void SyncCollidersToPhysics();

        // ECS to Physics: Manually moved objects (kinematic) sync to physics
        void SyncKinematicTransforms();

        // Physics to ECS: Physics-moved objects (dynamic) sync back to transforms
        void SyncPhysicsToTransforms();

        // Physics body management
        void CreatePhysicsBody(Entity entity);
        void UpdatePhysicsBody(Entity entity);
        void RecreatePhysicsBody(Entity entity);
    };
}