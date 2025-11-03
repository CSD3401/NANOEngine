#pragma once
#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../../Physics/PhysicsManager.hpp"
#include "../Components/Collider.hpp"
#include "../Components/Transform.hpp"

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

    private:
        ComponentManager* m_componentManager;

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