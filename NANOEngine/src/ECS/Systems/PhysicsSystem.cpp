#include "PhysicsSystem.hpp"
#include "../../Core/Profiler.hpp"
#include "../../ECS/Components/Transform.hpp"
#include "../../ECS/Components/Rigidbody.hpp"
#include "EngineState.hpp"

namespace NE::ECS::Systems
{
    // Initialize static member
    ComponentManager* PhysicsSystem::s_componentManager = nullptr;

    PhysicsSystem::PhysicsSystem(NE::ECS::ComponentManager* cm)
        : m_componentManager(cm)
    {
        // Set static pointer for helper methods (allows IScript to access physics)
        s_componentManager = cm;

        // Register collision callbacks
        NE::Physics::PhysicsManager::RegisterCollisionEnterCallback(
            [this](const NE::Physics::CollisionInfo& collision) {
                this->HandleCollisionEnter(collision);
            });

        NE::Physics::PhysicsManager::RegisterCollisionStayCallback(
            [this](const NE::Physics::CollisionInfo& collision) {
                this->HandleCollisionStay(collision);
            });

        NE::Physics::PhysicsManager::RegisterCollisionExitCallback(
            [this](const NE::Physics::CollisionInfo& collision) {
                this->HandleCollisionExit(collision);
            });
    }

    void PhysicsSystem::OnEntityAdded(Entity entity)
    {
        bool hasCollider = m_componentManager->HasComponent<Component::Collider>(entity);
        bool hasRigidbody = m_componentManager->HasComponent<Component::Rigidbody>(entity);
        bool hasTransform = m_componentManager->HasComponent<Component::Transform>(entity);

        if ((hasCollider || hasRigidbody) && hasTransform) {
            // Initialize collider callbacks if collider exists
            if (hasCollider) {
                auto& collider = m_componentManager->GetComponent<Component::Collider>(entity);

                // Register collision callbacks
                collider.onCollisionEnter = [entity](Entity otherEntity) {
                    printf("COLLISION ENTER: Entity %d collided with Entity %d\n", entity, otherEntity);
                    };

                collider.onCollisionStay = [entity](Entity otherEntity) {
                    printf("COLLISION STAY: Entity %d with Entity %d\n", entity, otherEntity);
                    };

                collider.onCollisionExit = [entity](Entity otherEntity) {
                    printf("COLLISION EXIT: Entity %d stopped colliding with Entity %d\n", entity, otherEntity);
                    };

                collider.previousShapeType = collider.shapeType;
                collider.previousHalfExtents = collider.halfExtents;
                collider.previousRadius = collider.radius;
                collider.previousHeight = collider.height;

                collider.isShapeDirty = false;
                collider.isPropertiesDirty = false;
            }

            if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity)) {
                CreatePhysicsBody(entity);
            }
        }
    }

    void PhysicsSystem::OnEntityRemoved(Entity entity)
    {
        if (NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity))
        {
            uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
            NE::Physics::PhysicsManager::DestroyBody(bodyID);
            NE::Physics::PhysicsManager::UnregisterEntityBody(entity);
        }
    }

    void PhysicsSystem::Init() {}

    void PhysicsSystem::Update(double dt)
    {
        SyncCollidersToPhysics();
        SyncKinematicTransforms();
        NE::Physics::PhysicsManager::Update(static_cast<float>(dt));
        SyncPhysicsToTransforms();
    }

    void PhysicsSystem::Exit() {}

    void PhysicsSystem::HandleCollisionEnter(const NE::Physics::CollisionInfo& collision)
    {
        if (m_componentManager->HasComponent<Component::Collider>(collision.entityA))
        {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(collision.entityA);
            if (collider.onCollisionEnter)
            {
                collider.onCollisionEnter(collision.entityB);
            }
        }

        if (m_componentManager->HasComponent<Component::Collider>(collision.entityB))
        {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(collision.entityB);
            if (collider.onCollisionEnter) {
                collider.onCollisionEnter(collision.entityA);
            }
        }
    }

    void PhysicsSystem::HandleCollisionStay(const NE::Physics::CollisionInfo& collision)
    {
        if (m_componentManager->HasComponent<Component::Collider>(collision.entityA))
        {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(collision.entityA);
            if (collider.onCollisionStay) {
                collider.onCollisionStay(collision.entityB);
            }
        }

        if (m_componentManager->HasComponent<Component::Collider>(collision.entityB))
        {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(collision.entityB);
            if (collider.onCollisionStay) {
                collider.onCollisionStay(collision.entityA);
            }
        }
    }

    void PhysicsSystem::HandleCollisionExit(const NE::Physics::CollisionInfo& collision)
    {
        if (m_componentManager->HasComponent<Component::Collider>(collision.entityA))
        {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(collision.entityA);
            if (collider.onCollisionExit) {
                collider.onCollisionExit(collision.entityB);
            }
        }

        if (m_componentManager->HasComponent<Component::Collider>(collision.entityB)) {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(collision.entityB);
            if (collider.onCollisionExit) {
                collider.onCollisionExit(collision.entityA);
            }
        }
    }

    void PhysicsSystem::SyncCollidersToPhysics()
    {
        auto colliderEntities = m_componentManager->GetEntitiesWithComponent<Component::Collider>();
        auto rigidbodyEntities = m_componentManager->GetEntitiesWithComponent<Component::Rigidbody>();

        std::unordered_set<Entity> entities(colliderEntities.begin(), colliderEntities.end());
        entities.insert(rigidbodyEntities.begin(), rigidbodyEntities.end());

        for (auto entity : entities) {
            if (m_componentManager->HasComponent<Component::Transform>(entity)) {
                if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity)) {
                    CreatePhysicsBody(entity);
                }
                else {
                    UpdatePhysicsBody(entity);
                }
            }
        }
    }

    void PhysicsSystem::SyncKinematicTransforms()
    {
        auto colliderEntities = m_componentManager->GetEntitiesWithComponent<Component::Collider>();
        auto rigidbodyEntities = m_componentManager->GetEntitiesWithComponent<Component::Rigidbody>();

        std::unordered_set<Entity> entities(colliderEntities.begin(), colliderEntities.end());
        entities.insert(rigidbodyEntities.begin(), rigidbodyEntities.end());

        for (auto entity : entities)
        {
            if (NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity)) {
                uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);

                if (NE::Physics::PhysicsManager::GetMotionType(bodyID) == JPH::EMotionType::Kinematic) {
                    auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
                    NE::Physics::PhysicsManager::SetTransform(bodyID, transform.position, transform.rotation);
                }
            }
        }
    }

    void PhysicsSystem::SyncPhysicsToTransforms()
    {

        auto colliderEntities = m_componentManager->GetEntitiesWithComponent<Component::Collider>();
        auto rigidbodyEntities = m_componentManager->GetEntitiesWithComponent<Component::Rigidbody>();

        std::unordered_set<Entity> entities(colliderEntities.begin(), colliderEntities.end());
        entities.insert(rigidbodyEntities.begin(), rigidbodyEntities.end());

        for (auto entity : entities)
        {
            if (NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity))
            {
                uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);
                JPH::EMotionType motionType = NE::Physics::PhysicsManager::GetMotionType(bodyID);

                // ONLY sync DYNAMIC bodies (physics controls them)
                if (motionType == JPH::EMotionType::Dynamic) {


                    Math::Vec3 position, rotation;
                    NE::Physics::PhysicsManager::GetTransform(bodyID, position, rotation);

                    auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

                    //printf("  OLD transform: (%.2f, %.2f, %.2f)\n",
                    //    transform.position.x, transform.position.y, transform.position.z);
                    //printf("  NEW physics:   (%.2f, %.2f, %.2f)\n",
                    //    position.x, position.y, position.z);

                    transform.position = position;
                    transform.rotation = rotation;
                    transform.isDirty = true;

                    //printf("  UPDATED! isDirty = %d\n", transform.isDirty);
                }
            }
        }
    }

    void PhysicsSystem::CreatePhysicsBody(Entity entity)
    {
        auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

        if (m_componentManager->HasComponent<Component::Collider>(entity)) {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(entity);
            collider.previousShapeType = collider.shapeType;
            collider.previousHalfExtents = collider.halfExtents;
            collider.previousRadius = collider.radius;
            collider.previousHeight = collider.height;

            collider.isShapeDirty = false;
            collider.isPropertiesDirty = false;
        }

        JPH::EMotionType motionType = JPH::EMotionType::Static;
        bool hasRigidbody = m_componentManager->HasComponent<Component::Rigidbody>(entity);

        if (hasRigidbody)
        {
            auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);

            if (NE::GetEngineState() != EngineState::Play) {
                motionType = JPH::EMotionType::Kinematic;
                printf("PhysicsSystem: EDITOR MODE - Setting body to KINEMATIC for entity %d\n", entity);
            }
            else {
                motionType = rb.isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
                printf("PhysicsSystem: PLAY MODE - Setting body to %s for entity %d\n",
                    rb.isStatic ? "STATIC" : "DYNAMIC", entity);
            }
        }
        else {
            if (NE::GetEngineState() != EngineState::Play) {
                motionType = JPH::EMotionType::Kinematic;
                printf("PhysicsSystem: EDITOR MODE - Collider-only, setting to KINEMATIC for entity %d\n", entity);
            }
        }

        uint32_t bodyID = 0;

        if (m_componentManager->HasComponent<Component::Collider>(entity)) {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(entity);

            switch (collider.shapeType) {
            case Component::Collider::ShapeType::Box:
                bodyID = NE::Physics::PhysicsManager::CreateBoxBody(
                    transform.position, transform.rotation,
                    collider.halfExtents * 2.0f,
                    motionType
                );
                break;

            case Component::Collider::ShapeType::Sphere:
                bodyID = NE::Physics::PhysicsManager::CreateSphereBody(
                    transform.position, transform.rotation,
                    collider.radius,
                    motionType
                );
                break;

            case Component::Collider::ShapeType::Capsule:
                bodyID = NE::Physics::PhysicsManager::CreateCapsuleBody(
                    transform.position, transform.rotation,
                    collider.height * 0.5f,
                    collider.radius,
                    motionType
                );
                break;

            case Component::Collider::ShapeType::None:
                break;
            }
        }
        else {
            Math::Vec3 defaultSize(1.0f, 1.0f, 1.0f);
            bodyID = NE::Physics::PhysicsManager::CreateBoxBody(
                transform.position, transform.rotation,
                defaultSize,
                motionType
            );
        }

        if (bodyID != 0)
        {
            NE::Physics::PhysicsManager::RegisterEntityBody(entity, bodyID);

            if (hasRigidbody) {
                auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
                rb.bodyID = bodyID;
            }

            printf("PhysicsSystem: Created %s body %d for entity %d\n",
                motionType == JPH::EMotionType::Kinematic ? "KINEMATIC" :
                motionType == JPH::EMotionType::Dynamic ? "DYNAMIC" : "STATIC",
                bodyID, entity);
        }
    }

    void PhysicsSystem::UpdatePhysicsBody(Entity entity)
    {
        if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity)) {
            return;
        }

        uint32_t bodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);

        if (m_componentManager->HasComponent<Component::Rigidbody>(entity)) {
            auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
            JPH::EMotionType currentMotionType = NE::Physics::PhysicsManager::GetMotionType(bodyID);

            JPH::EMotionType desiredMotionType;
            if (NE::GetEngineState() != EngineState::Play) {
                desiredMotionType = JPH::EMotionType::Kinematic;
            }
            else {
                desiredMotionType = rb.isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
            }

            if (currentMotionType != desiredMotionType) {
                // When transitioning, sync FROM physics TO transform
                Math::Vec3 currentPhysicsPos, currentPhysicsRot;
                NE::Physics::PhysicsManager::GetTransform(bodyID, currentPhysicsPos, currentPhysicsRot);

                auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);
                transform.position = currentPhysicsPos;
                transform.rotation = currentPhysicsRot;

                NE::Physics::PhysicsManager::SetMotionType(bodyID, desiredMotionType);
            }
        }

        // Handle collider changes (only in editor)
        if (NE::GetEngineState() != EngineState::Play &&
            m_componentManager->HasComponent<Component::Collider>(entity))
        {
            auto& collider = m_componentManager->GetComponent<Component::Collider>(entity);

            if (collider.isShapeDirty || collider.isPropertiesDirty) {
                printf("Collider properties changed for entity %d\n", entity);

                collider.isShapeDirty = false;
                collider.isPropertiesDirty = false;

                RecreatePhysicsBody(entity);
            }
        }
    }

    void PhysicsSystem::RecreatePhysicsBody(Entity entity)
    {
        if (!NE::Physics::PhysicsManager::EntityHasPhysicsBody(entity)) {
            return;
        }

        uint32_t oldBodyID = NE::Physics::PhysicsManager::GetEntityBodyId(entity);

        NE::Physics::PhysicsManager::DestroyBody(oldBodyID);
        NE::Physics::PhysicsManager::UnregisterEntityBody(entity);

        if (m_componentManager->HasComponent<Component::Rigidbody>(entity)) {
            auto& rb = m_componentManager->GetComponent<Component::Rigidbody>(entity);
            rb.bodyID = 0;
        }

        CreatePhysicsBody(entity);

        printf("PhysicsSystem: Recreated physics body for entity %d\n", entity);
    }
}