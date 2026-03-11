#pragma once

#include <string>
#include <memory>

//#include "Animation/AnimatorController.hpp"
#include "Animation/AnimationClip.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct Animator {
        enum UpdateMode : uint8_t {
            Normal,
            Fixed,
            UnscaledTime
		};

        enum CullingMode : uint8_t {
            AlwaysAnimate,
            CullUpdateTransforms,
            CullCompletely
		};

        //std::string animControllerUUID;
        std::string animClipUUID;
        uint64_t luid = 0;
		//std::shared_ptr<Animation::AnimatorController> controller;
		std::shared_ptr<Animation::AnimationClip> clip; // for now
		bool applyRootMotion = false;
		bool animatePhysics = false;
		bool playOnStart = false;
        UpdateMode updateMode = UpdateMode::Normal;
		CullingMode cullingMode = CullingMode::AlwaysAnimate;

		bool isPlaying = false;
        float time = 0.0f;
        float speed = 1.0f;
        float prevTime = 0.0f;

        NE_REFLECT_BEGIN(Animator)
            NE_REFLECT_FIELD_HIDDEN(animClipUUID),
            //NE_REFLECT_FIELD_NAMED(applyRootMotion, "Apply Root Motion"),
            NE_REFLECT_FIELD_HIDDEN(applyRootMotion),
            //NE_REFLECT_FIELD_NAMED(animatePhysics, "Animate Physics"),
            NE_REFLECT_FIELD_HIDDEN(animatePhysics),
            NE_REFLECT_FIELD_NAMED(playOnStart, "Play On Start"),
            NE_REFLECT_FIELD_HIDDEN(updateMode),
            NE_REFLECT_FIELD_HIDDEN(cullingMode),
            NE_REFLECT_FIELD_HIDDEN(luid)
            NE_REFLECT_END()
    };
}
