#include "AnimatorSystem.hpp"
#include "../../Animation/AnimatorControllerIO.hpp"
#include "../../Animation/TransformClipIO.hpp"
#include <filesystem>
#include <cmath> // for std::fmod
using namespace NE::Animation;

namespace NE::ECS::Systems {
    static NE::Math::Vec3 Lerp(const NE::Math::Vec3& a, const NE::Math::Vec3& b, float t) {
        return a + (b - a) * t;
    }
    static void ApplyBlended(
        NE::ECS::Component::Transform& tr,
        const NE::Animation::TransformClip* A, float tA,
        const NE::Animation::TransformClip* B, float tB,
        float alpha, bool loop)
    {
        using NE::Animation::TransformClip;
        if (!A && !B) return;
        auto sample = [&](const NE::Animation::TransformClip* C, const std::vector<NE::Animation::KeyframeVec3>& ch, const NE::Math::Vec3& fallback) {
            if (!C || ch.empty()) return fallback;
            return TransformClip::Sample(ch, C == A ? tA : tB, loop);
            };
        NE::Math::Vec3 pA = sample(A, A ? A->pos : std::vector<NE::Animation::KeyframeVec3>{}, tr.localPosition);
        NE::Math::Vec3 rA = sample(A, A ? A->rot : std::vector<NE::Animation::KeyframeVec3>{}, tr.localRotationEuler);
        NE::Math::Vec3 sA = sample(A, A ? A->scl : std::vector<NE::Animation::KeyframeVec3>{}, tr.localScale);

        NE::Math::Vec3 pB = sample(B, B ? B->pos : std::vector<NE::Animation::KeyframeVec3>{}, pA);
        NE::Math::Vec3 rB = sample(B, B ? B->rot : std::vector<NE::Animation::KeyframeVec3>{}, rA);
        NE::Math::Vec3 sB = sample(B, B ? B->scl : std::vector<NE::Animation::KeyframeVec3>{}, sA);

        tr.localPosition = Lerp(pA, pB, alpha);
        tr.localRotationEuler = Lerp(rA, rB, alpha); // Euler lerp (simple). Replace with quat slerp if you add quats later.
        tr.localScale = Lerp(sA, sB, alpha);
    }

    // inside namespace NE::ECS::Systems
    //std::shared_ptr<NE::Animation::AnimatorController>
    //    AnimatorSystem::LoadOrGetController(const std::string& path) {
    //    // Already cached?
    //    auto it = m_controllers.find(path);
    //    if (it != m_controllers.end())
    //        return it->second;

    //    // Load from disk if the path points to a real file
    //    if (!path.empty() && std::filesystem::is_regular_file(path)) {
    //        auto ctrl = std::make_shared<NE::Animation::AnimatorController>();
    //        if (NE::Animation::LoadAnimatorController(*ctrl, path)) {
    //            m_controllers[path] = ctrl;
    //            return ctrl;
    //        }
    //    }
    //    return nullptr;
    //}

    //std::shared_ptr<NE::Animation::TransformClip>
    //    AnimatorSystem::LoadOrGetClip(const std::string& id) {
    //    // Already cached?
    //    auto it = m_clips.find(id);
    //    if (it != m_clips.end())
    //        return it->second;

    //    // Treat the id as a file path (your controller uses file paths for clipId)
    //    if (!id.empty() && std::filesystem::is_regular_file(id)) {
    //        auto clip = std::make_shared<NE::Animation::TransformClip>();
    //        if (NE::Animation::LoadTransformClip(*clip, id)) {
    //            m_clips[id] = clip;
    //            return clip;
    //        }
    //    }
    //    return nullptr;
    //}


    void AnimatorSystem::Update(double deltaTime)
    {
        //using namespace NE::Animation;

        //for (auto e : GetEntities()) {
        //    auto* animator = &m_componentManager->GetComponent<Component::Animator>(e);
        //    auto* transform = &m_componentManager->GetComponent<Component::Transform>(e);
        //    if (!animator || !transform) continue;

        //    if (animator->playOnStart && !animator->playing) animator->playing = true;
        //    if (!animator->playing) continue;

        //    // -------- Controller path --------
        //    if (!animator->controllerPath.empty()) {
        //        EnsureControllerLoaded(animator->controllerPath);
        //        auto ctrlPtr = LoadOrGetController(animator->controllerPath);
        //        if (!ctrlPtr) continue;
        //        const AnimatorController& ctrl = *ctrlPtr;


        //        AnimatorInstance& inst = m_instances[e]; // default-constructed if new
        //        if (inst.timeInState == 0.0f && inst.currentState == 0 && !ctrl.states.empty()) {
        //            inst.currentState = std::min(ctrl.defaultState, (uint32_t)ctrl.states.size() - 1);
        //        }
        //        if (ctrl.states.empty()) continue;

        //        const State& S = ctrl.states[inst.currentState];
        //        auto clipA = LoadOrGetClip(S.clipId);
        //        if (!clipA) { continue; }

        //        // Apply pending triggers BEFORE evaluating transitions so they can fire this frame.
        //        for (const auto& trigName : animator->setTriggers) { inst.triggers[trigName] = true; }
        //        animator->setTriggers.clear();

        //        // Advance time in current state
        //        float dt = static_cast<float>(deltaTime) * S.speed * animator->speed;
        //        inst.timeInState += dt;

        //        const float clipLen = std::max(clipA->length, 0.0001f);

        //        // Condition evaluator
        //        auto meets = [&](const Condition& c)->bool {
        //            auto itp = std::find_if(ctrl.parameters.begin(), ctrl.parameters.end(),
        //                [&](const Parameter& p) { return p.name == c.param; });
        //            if (itp == ctrl.parameters.end()) return false;
        //            const Parameter& P = *itp;

        //            auto getB = [&]() { auto it = animator->bools.find(P.name);   return it != animator->bools.end() ? it->second : P.b; };
        //            auto getF = [&]() { auto it = animator->floats.find(P.name);  return it != animator->floats.end() ? it->second : P.f; };
        //            auto getI = [&]() { auto it = animator->ints.find(P.name);    return it != animator->ints.end() ? it->second : P.i; };
        //            auto getT = [&]() { auto it = inst.triggers.find(P.name);     return it != inst.triggers.end() ? it->second : false; };

        //            switch (P.type) {
        //            case ParamType::Bool:
        //                if (c.op == CondOp::If)    return getB();
        //                if (c.op == CondOp::IfNot) return !getB();
        //                return false;
        //            case ParamType::Trigger:
        //                if (c.op == CondOp::If)    return getT();
        //                if (c.op == CondOp::IfNot) return !getT();
        //                return false;
        //            case ParamType::Float:
        //                if (c.op == CondOp::Greater) return getF() > c.f;
        //                if (c.op == CondOp::Less)    return getF() < c.f;
        //                if (c.op == CondOp::Equals)  return std::fabs(getF() - c.f) < 1e-6f;
        //                if (c.op == CondOp::NotEquals)return std::fabs(getF() - c.f) >= 1e-6f;
        //                return false;
        //            case ParamType::Int:
        //                if (c.op == CondOp::Equals)     return getI() == c.i;
        //                if (c.op == CondOp::NotEquals)  return getI() != c.i;
        //                if (c.op == CondOp::Greater)    return getI() > c.i;
        //                if (c.op == CondOp::Less)       return getI() < c.i;
        //                return false;
        //            }
        //            return false;
        //            };

        //        // Check transitions (first match wins)
        //        for (const auto& T : S.transitions) {
        //            if (T.hasExitTime) {
        //                float localT = animator->loop ? std::fmod(inst.timeInState, clipLen) : inst.timeInState;
        //                if (localT < 0.0f) localT += clipLen;
        //                float normalized = localT / clipLen;
        //                if (normalized < T.exitTimeNormalized) continue;
        //            }
        //            bool ok = true;
        //            for (const auto& C : T.conditions) { if (!(ok &= meets(C))) break; }
        //            if (!ok) continue;

        //            if (!T.canTransitionToSelf && T.toState == inst.currentState) continue;
        //            if (T.toState >= ctrl.states.size()) continue;

        //            // Begin crossfade
        //            inst.inTransition = (T.duration > 0.0f);
        //            inst.nextState = T.toState;
        //            inst.transitionElapsed = 0.0f;
        //            inst.transitionDuration = std::max(0.0001f, T.duration);
        //            break; // first valid transition wins
        //        }

        //        // Pose application
        //        if (inst.inTransition) {
        //            const State& N = ctrl.states[inst.nextState];
        //            auto clipB = LoadOrGetClip(N.clipId);

        //            float tA = inst.timeInState;
        //            float tB = (inst.transitionElapsed == 0.0f) ? 0.0f : inst.timeInState * (N.speed / S.speed);

        //            inst.transitionElapsed += static_cast<float>(deltaTime);
        //            float alpha = std::min(1.0f, inst.transitionElapsed / inst.transitionDuration);

        //            ApplyBlended(*transform, clipA.get(), tA, clipB.get(), tB, alpha, animator->loop);

        //            if (alpha >= 1.0f) {
        //                inst.inTransition = false;
        //                inst.currentState = inst.nextState;
        //                inst.timeInState = tB;
        //                // consume triggers after a successful transition
        //                for (auto& kv : inst.triggers) kv.second = false;
        //            }
        //        }
        //        else {
        //            // Single clip sample (current state)
        //            clipA->ApplyTo(*transform, inst.timeInState, animator->loop);
        //        }

        //        continue; // controller path handled this entity
        //    }

        //    // -------- Single-clip fallback path --------
        //    animator->time += static_cast<float>(deltaTime) * animator->speed;
        //    if (!animator->activeClip.empty()) {
        //        auto clipPtr = GetClip(animator->activeClip);
        //        if (clipPtr) {
        //            const float end = clipPtr->length;
        //            float t = animator->time;
        //            if (animator->loop) {
        //                if (end > 0.0f) t = static_cast<float>(std::fmod(t, end));
        //                if (t < 0.0f) t += end;
        //            }
        //            else if (t > end) {
        //                t = end;
        //                animator->playing = false;
        //            }
        //            clipPtr->ApplyTo(*transform, t, animator->loop);
        //        }
        //    }
        //}
    }

    //void AnimatorSystem::EnsureControllerLoaded(const std::string& path) {
        //namespace fs = std::filesystem;
        //if (path.empty()) return;

        //fs::file_time_type mtime{};
        //bool haveFile = false;
        //if (fs::exists(path) && fs::is_regular_file(path)) {
        //    mtime = fs::last_write_time(path);
        //    haveFile = true;
        //}

        //auto needReload =
        //    (m_controllers.find(path) == m_controllers.end()) ||
        //    (m_ctrlMTime.find(path) == m_ctrlMTime.end()) ||
        //    (haveFile && m_ctrlMTime[path] != mtime);

        //if (!needReload) return;

        //auto ctrl = std::make_shared<AnimatorController>();
        //if (haveFile && LoadAnimatorController(*ctrl, path)) {
        //    m_controllers[path] = ctrl;
        //    m_ctrlMTime[path] = mtime;

        //    // Safety: if states shrank, clamp/repair instances
        //    for (auto& kv : m_instances) {
        //        auto& inst = kv.second;
        //        if (ctrl->states.empty()) { inst.currentState = 0; continue; }
        //        if (inst.currentState >= ctrl->states.size())
        //            inst.currentState = std::min<uint32_t>(ctrl->defaultState, (uint32_t)ctrl->states.size() - 1);
        //        if (inst.nextState >= ctrl->states.size())
        //            inst.inTransition = false;
        //    }
        //    // (optional) log: spdlog::info("Animator: reloaded {}", path);
        //}
        //else {
        //    // Failed to load: keep previous valid controller if any, just update mtime slot
        //    m_ctrlMTime[path] = mtime;
        //    // (optional) log error
        //}
    //}


} // namespace
