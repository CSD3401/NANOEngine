#include "AnimatorSystem.hpp"
#include <cmath> // for std::fmod

namespace NE::ECS::Systems {


    void AnimatorSystem::Update(double deltaTime)
    {
        for (auto e : GetEntities()) {
            auto* animator = &m_componentManager->GetComponent<Component::Animator>(e);
            auto* transform = &m_componentManager->GetComponent<Component::Transform>(e);
            if (!animator || !transform) continue;

            if (animator->playOnStart && !animator->playing) animator->playing = true;
            if (!animator->playing) continue;

            animator->time += static_cast<float>(deltaTime) * animator->speed;

            if (!animator->activeClip.empty()) {
                auto clipPtr = GetClip(animator->activeClip);  // rename to avoid any macro collisions
                if (clipPtr) {
                    const float end = clipPtr->length;
                    float t = animator->time;
                    if (animator->loop) {
                        if (end > 0.0f) t = static_cast<float>(std::fmod(t, end));
                        if (t < 0.0f) t += end;
                    }
                    else if (t > end) {
                        t = end;
                        animator->playing = false;
                    }
                    clipPtr->ApplyTo(*transform, t, animator->loop);
                }
            }
        }
    }


} // namespace
