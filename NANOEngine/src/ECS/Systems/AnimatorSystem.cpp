#include "AnimatorSystem.hpp"

#include <algorithm>

#include "ECS/Core/EntityManager.hpp"
#include "ECS/Core/ComponentManager.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Math/Quat.hpp"

#include "ECS/Components/EntityMeta.hpp"
#include "ECS/Components/Animator.hpp"
#include "ECS/Components/Transform.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/Light.hpp"

#include "ResourceManagement/ResourceManager.hpp"
#include "Animation/AnimationClip.hpp"

namespace NE::ECS::Systems {
    namespace {
        float AdvanceTime(float t, float dt, const Animation::AnimationClip& clip) {
            float L = clip.GetLengthSeconds();
            if (L <= 0.0f) return std::max(0.0f, t + dt);

            t += dt;

            if (clip.IsLooping()) {
                t = std::fmod(t, L);
                if (t < 0.0f) t += L;
                return t;
            }
            return std::clamp(t, 0.0f, L);
        }

        float EvalCurveLinear(const NE::Animation::AnimCurveF& c, float t) {
            const auto& keys = c.keys;
            if (keys.empty()) return 0.0f;
            if (keys.size() == 1) return keys[0].value;

            if (t <= keys.front().time) return keys.front().value;
            if (t >= keys.back().time)  return keys.back().value;

            for (size_t i = 0; i + 1 < keys.size(); ++i) {
                const auto& a = keys[i];
                const auto& b = keys[i + 1];
                if (t >= a.time && t <= b.time) {
                    const float dt = (b.time - a.time);
                    if (dt <= 1e-6f) return a.value;
                    const float u = (t - a.time) / dt;
                    return a.value + (b.value - a.value) * u;
                }
            }
            return keys.back().value;
        }

        template <typename FieldT>
        bool SampleTrackToValue(const NE::Animation::AnimTrack& tr, float t, FieldT& out) {
            using VT = NE::Animation::AnimValueType;

            if constexpr (std::is_same_v<FieldT, bool>) {
                if (tr.type != VT::Bool) return false;
                out = (EvalCurveLinear(tr.x, t) >= 0.5f);
                return true;
            } else if constexpr (std::is_same_v<FieldT, float>) {
                if (tr.type != VT::Float) return false;
                out = EvalCurveLinear(tr.x, t);
                return true;
            } else if constexpr (std::is_same_v<FieldT, NE::Math::Vec2>) {
                if (tr.type != VT::Vec2) return false;
                out.x = EvalCurveLinear(tr.x, t);
                out.y = EvalCurveLinear(tr.y, t);
                return true;
            } else if constexpr (std::is_same_v<FieldT, NE::Math::Vec3>) {
                if (tr.type != VT::Vec3) return false;
                out.x = EvalCurveLinear(tr.x, t);
                out.y = EvalCurveLinear(tr.y, t);
                out.z = EvalCurveLinear(tr.z, t);
                return true;
            } else if constexpr (std::is_same_v<FieldT, NE::Math::Vec4>) {
                if (tr.type != VT::Vec4) return false;
                out.x = EvalCurveLinear(tr.x, t);
                out.y = EvalCurveLinear(tr.y, t);
                out.z = EvalCurveLinear(tr.z, t);
                out.w = EvalCurveLinear(tr.w, t);
                return true;
            } else {
                return false;
            }
        }

        uint32_t FNV1a32(std::string_view s) {
            uint32_t h = 2166136261u;
            for (unsigned char c : s) { h ^= c; h *= 16777619u; }
            return h;
        }

        static uint32_t MakeFieldId(const char* componentName, std::string_view fieldName) {
            std::string full;
            full.reserve(std::strlen(componentName) + 1 + fieldName.size());
            full.append(componentName);
            full.push_back('.');
            full.append(fieldName.data(), fieldName.size());
            return FNV1a32(full);
        }

        void MarkDirtyIfPresent(auto& comp) {
            if constexpr (requires { comp.isDirty; }) comp.isDirty = true;
        }

        template <typename CompT>
        bool ApplyTrackToComponent(uint32_t e,
			NE::ECS::ComponentManager* m_componentManager,
            const NE::Animation::AnimTrack& tr,
            float t,
            const char* componentName)
        {
            if (!m_componentManager->HasComponent<CompT>(e))
                return false;

            auto& comp = m_componentManager->GetComponent<CompT>(e);

            bool applied = false;

            NE::Core::ForEachField(comp, [&](auto&& desc, auto&& fieldRef) {
                if (applied) return;

                const uint32_t fid = MakeFieldId(componentName, desc.name);
                if (fid != tr.fieldId) return;

                using FieldT = std::remove_cvref_t<decltype(fieldRef)>;

                FieldT sampled{};
                if (!SampleTrackToValue<FieldT>(tr, t, sampled))
                    return;

                fieldRef = sampled;
                applied = true;
                }
            );

            if constexpr (requires { comp.localRotationQuat; }) 
                comp.localRotationQuat = NE::Math::Quat::FromEulerDegrees(comp.localRotationEuler);

            MarkDirtyIfPresent(comp);

            return applied;
        }
    }

    AnimatorSystem::AnimatorSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr) 
        : m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) {}

    void AnimatorSystem::OnEntityAdded(Entity e) {
        auto& anim = m_componentManager->GetComponent<Component::Animator>(e);

        if (!anim.animClipUUID.empty() && !anim.clip) {
            anim.clip = Resource::ResourceManager::GetInstance().
                LoadResource<Animation::AnimationClip>(anim.animClipUUID);
		}

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
        const auto& entities = m_entities.GetDenseContainer();


        for (Entity entity : entities) {
            auto& anim = m_componentManager->GetComponent<Component::Animator>(entity);

            if (!anim.isPlaying)
                continue;

            if (!anim.clip) continue;

            float dt = static_cast<float>(deltaTime);

            anim.prevTime = anim.time;
            anim.time = AdvanceTime(anim.time, dt * anim.speed, *anim.clip);

            ApplyClipAtTime(entity, *anim.clip, anim.time);
        }
    }

    void AnimatorSystem::Exit() {

    }

    void AnimatorSystem::ApplyClipAtTime(uint32_t e, const Animation::AnimationClip& clip, float t) {
        for (const auto& tr : clip.GetTracks()) {
            if (tr.componentTypeId == m_componentManager->GetComponentType<Component::EntityMeta>()) {
                ApplyTrackToComponent<NE::ECS::Component::EntityMeta>(e, m_componentManager, tr, t, "EntityMeta");
            } else if (tr.componentTypeId == m_componentManager->GetComponentType<Component::Transform>()) {
                ApplyTrackToComponent<NE::ECS::Component::Transform>(e, m_componentManager, tr, t, "Transform");
            } else if (tr.componentTypeId == m_componentManager->GetComponentType<Component::Renderer>()) {
                ApplyTrackToComponent<NE::ECS::Component::Renderer>(e, m_componentManager, tr, t, "Renderer");
            } else if (tr.componentTypeId == m_componentManager->GetComponentType<Component::Light>()) {
                ApplyTrackToComponent<NE::ECS::Component::Light>(e, m_componentManager, tr, t, "Light");
            }
        }
    }
}
