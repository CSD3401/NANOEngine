#include "AnimatorSystem.hpp"

#include "ECS/Core/EntityManager.hpp"
#include "ECS/Core/ComponentManager.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Core/LUIDGenerator.hpp"

#include "ECS/Components/EntityMeta.hpp"
#include "ECS/Components/Animator.hpp"

#include "Animation/AnimationClip.hpp"

namespace NE::ECS::Systems {

    AnimatorSystem::AnimatorSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr) 
        : m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) {}

    void AnimatorSystem::OnEntityAdded(Entity e) {
        auto& anim = m_componentManager->GetComponent<Component::Animator>(e);

        if (anim.luid == 0)
            anim.luid = Core::LUIDGenerator::Generate("ac");

        m_luidRegistry->Register(anim.luid, &anim, e);
    }

    void AnimatorSystem::OnEntityRemoved(Entity e) {
        auto& anim = m_componentManager->GetComponent<Component::Animator>(e);
        m_luidRegistry->Unregister(anim.luid);
    }

    void AnimatorSystem::Init() {

    }

    void AnimatorSystem::Update(double deltaTime) {

    }

    void AnimatorSystem::Exit() {

    }

} // namespace
