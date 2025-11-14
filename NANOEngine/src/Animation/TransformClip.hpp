#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "../Math/Vec3.hpp"
#include "../ECS/Components/Transform.hpp"

namespace NE::Animation {

    struct KeyframeVec3 {
        float t = 0.0f;         // seconds
        NE::Math::Vec3 v{};
    };

    // Very small "Transform-only" animation clip that can animate TRS.
    struct TransformClip {
        std::string name = "NewClip";
        float length     = 1.0f;           // seconds
        std::vector<KeyframeVec3> pos;     // local position keys
        std::vector<KeyframeVec3> rot;     // euler degrees keys
        std::vector<KeyframeVec3> scl;     // local scale keys

        static NE::Math::Vec3 Sample(const std::vector<KeyframeVec3>& keys, float t, bool loop)
        {
            if (keys.empty()) return {};
            if (loop && lengthSafe(keys) > 0.0f) {
                float L = lengthSafe(keys);
                t = fmodf(t, L);
                if (t < 0) t += L;
            }
            // clamp
            if (t <= keys.front().t) return keys.front().v;
            if (t >= keys.back().t)  return keys.back().v;
            // find interval
            auto it = std::upper_bound(keys.begin(), keys.end(), t,
                [](float tt, const KeyframeVec3& k){ return tt < k.t; });
            size_t i1 = size_t(std::distance(keys.begin(), it));
            size_t i0 = i1 - 1;
            const auto& k0 = keys[i0];
            const auto& k1 = keys[i1];
            float a = (t - k0.t) / std::max(0.0001f, (k1.t - k0.t));
            return k0.v * (1.0f - a) + k1.v * a;
        }

        void ApplyTo(NE::ECS::Component::Transform& tr, float t, bool loop) const
        {
            if (!pos.empty()) tr.position = Sample(pos, t, loop);
            if (!rot.empty()) tr.rotation = Sample(rot, t, loop);
            if (!scl.empty()) tr.scale    = Sample(scl, t, loop);
        }

        static float lengthSafe(const std::vector<KeyframeVec3>& keys) {
            if (keys.empty()) return 0.0f;
            return keys.back().t - keys.front().t;
        }
    };

} // namespace NE::Animation
