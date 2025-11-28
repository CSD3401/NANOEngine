#pragma once

#include "Core/Reflection.hpp"
#include "Math/Vec3.hpp"

namespace NE::Graphics {

	struct BloomSettings {
		enum ToneMapType {
			Reinhard,
			ReinhardExtended,
			ACESApproximation,
			FilmicACES
		};

		Math::Vec3 tint;

		float brightThreshold = 1.f;
		float brightScale = 1.f;
		float softKnee = 0.2f;
		float bloomRadius;
		float bloomIntensity;
		float exposure;

		ToneMapType toneMapType = ToneMapType::Reinhard;

		NE_REFLECT_BEGIN(BloomSettings)
			NE_REFLECT_FIELD(tint),
			NE_REFLECT_FIELD(brightThreshold),
			NE_REFLECT_FIELD(brightScale),
			NE_REFLECT_FIELD(softKnee),
			NE_REFLECT_FIELD(bloomRadius),
			NE_REFLECT_FIELD(bloomIntensity),
			NE_REFLECT_FIELD(exposure)
		NE_REFLECT_END()
	};

	struct PostProcessingSettings {
		BloomSettings bloomSettings;

		NE_REFLECT_BEGIN(PostProcessingSettings)
			NE_REFLECT_FIELD(bloomSettings)
		NE_REFLECT_END()
	};

}
