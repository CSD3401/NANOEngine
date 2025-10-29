// NANOEngine/src/ECS/Components/Animator.hpp
#pragma once
#include <string>
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct Animator {
        bool  playOnStart = false;
        bool  loop = true;
        float speed = 1.0f;

        // Persisted identifier (we'll store the clip file path here)
        std::string activeClip;

        // Non-persisted runtime
        bool  playing = false;
        float time = 0.0f;

        NE_REFLECT_BEGIN(Animator)
            NE_REFLECT_FIELD_NAMED(playOnStart, "Play On Start"),
            NE_REFLECT_FIELD_NAMED(loop, "Loop"),
            NE_REFLECT_FIELD_NAMED(speed, "Speed"),
            NE_REFLECT_FIELD_NAMED(activeClip, "Active Clip")
            NE_REFLECT_END()
    };
}
