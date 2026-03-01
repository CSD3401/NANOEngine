#pragma once

#include <string>
#include <vector>

#include "NANOEngineAPI.hpp"
#include "Core/Reflection.hpp"
#include "ResourceManagement/IResource.hpp"
#include "ResourceManagement/BinaryView.hpp"
#include "ResourceManagement/BinaryHeaders/NanoAnimClipHeader.hpp"

namespace NE::Animation {
    struct AnimKeyF {
        float time;      // seconds
        float value;
        float inTan;     // for cubic
        float outTan;

        NE_REFLECT_BEGIN(AnimKeyF)
            NE_REFLECT_FIELD(time),
            NE_REFLECT_FIELD(value),
            NE_REFLECT_FIELD(inTan),
            NE_REFLECT_FIELD(outTan)
            NE_REFLECT_END()
    };

    struct AnimCurveF {
        std::vector<AnimKeyF> keys;
        // plus interpolation mode: Step/Linear/Cubic
        NE_REFLECT_BEGIN(AnimCurveF)
            NE_REFLECT_FIELD(keys)
            NE_REFLECT_END()
    };

    struct AnimKeyS {
        float time;
        std::string value;

        NE_REFLECT_BEGIN(AnimKeyS)
            NE_REFLECT_FIELD(time),
            NE_REFLECT_FIELD(value)
            NE_REFLECT_END()
    };

    struct AnimCurveS {
        std::vector<AnimKeyS> keys;
        NE_REFLECT_BEGIN(AnimCurveS)
            NE_REFLECT_FIELD(keys)
            NE_REFLECT_END()
    };

    enum class AnimValueType { Bool, Float, Vec2, Vec3, Vec4, Quat, String };

    struct AnimTrack {
        std::string relativePath; // "Root/Spine/Arm_L" or your LUID path
        uint32_t componentTypeId;
        uint32_t fieldId;
        AnimValueType type;

        // curves (fast + simple)
        AnimCurveF x, y, z, w;    // used depending on type
        AnimCurveS s;             // used by String tracks

        NE_REFLECT_BEGIN(AnimTrack)
            NE_REFLECT_FIELD(relativePath),
            NE_REFLECT_FIELD(componentTypeId),
            NE_REFLECT_FIELD(fieldId),
            NE_REFLECT_FIELD(type),
            NE_REFLECT_FIELD(x),
            NE_REFLECT_FIELD(y),
            NE_REFLECT_FIELD(z),
            NE_REFLECT_FIELD(w),
            NE_REFLECT_FIELD(s)
			NE_REFLECT_END()
    };

    struct AnimClipBlob {
        std::string name;
        float lengthSeconds = 0.0f;
        bool looping = false;
        std::vector<AnimTrack> tracks;

        NE_REFLECT_BEGIN(AnimClipBlob)
            NE_REFLECT_FIELD(name),
            NE_REFLECT_FIELD(lengthSeconds),
            NE_REFLECT_FIELD(looping),
            NE_REFLECT_FIELD(tracks)
            NE_REFLECT_END()
    };


	class NANOENGINE_API AnimationClip final : public Resource::IResource {
    public:
		bool Preload(NE::Resource::BinaryView blob) override;
		void Finalize() override;
        static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::AnimationClip; }
		Resource::ResourceType GetType() const override { return GetStaticType(); }

        const std::string& GetName() const;
        float GetLengthSeconds() const;
		void SetLengthSeconds(float len);
        bool IsLooping() const;

        std::vector<AnimTrack>& GetTracksMutable();
        const std::vector<AnimTrack>& GetTracks() const;
    private:
        struct ParsedAnimClip {
            const NE::Resource::NanoAnimClipHeader* hdr = nullptr;
            const uint8_t* base = nullptr;
            size_t size = 0;
        };
        ParsedAnimClip m_stage;

        std::string name;
        float lengthSeconds;
        bool looping;

        std::vector<AnimTrack> tracks;
	};
}
