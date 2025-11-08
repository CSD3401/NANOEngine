// NANOEngine/src/ECS/Components/Animator.hpp
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct Animator {
        bool  playOnStart = false;
        bool  loop = true;
        float speed = 1.0f;

        // Persisted identifier (existing)
        std::string activeClip;

        // NEW: controller asset path (persisted)
        std::string controllerPath;

        // Non-persisted runtime
        bool  playing = false;
        float time = 0.0f;

        // NEW: runtime parameter overrides (non-persisted)
        std::unordered_map<std::string, bool>  bools;
        std::unordered_map<std::string, float> floats;
        std::unordered_map<std::string, int>   ints;
        std::vector<std::string>               setTriggers; // push names; system consumes

        NE_REFLECT_BEGIN(Animator)
            NE_REFLECT_FIELD_NAMED(playOnStart, "Play On Start"),
            NE_REFLECT_FIELD_NAMED(loop, "Loop"),
            NE_REFLECT_FIELD_NAMED(speed, "Speed"),
            NE_REFLECT_FIELD_NAMED(activeClip, "Active Clip"),
            NE_REFLECT_FIELD_NAMED(controllerPath, "Controller")
            NE_REFLECT_END()
    };
}
