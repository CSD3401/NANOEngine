#pragma once

#include "Core/Reflection.hpp"
#include "Math/Vec3.hpp"

namespace NE::Graphics {

	struct BloomSettings {
		bool enabled = true;

		enum ToneMapType {
			Reinhard,
			ReinhardExtended,
			ACESApproximation,
			FilmicACES,
			ACESFitted
		};

		Math::Vec3 tint{ 1.f, 1.f, 1.f };

		float brightThreshold = 1.f;
		float brightScale = 1.f;
		float softKnee = 0.5f;
		float bloomRadius = 1.f;
		float bloomIntensity = 0.06f;
		float exposure = 1.f;

		ToneMapType toneMapType = ToneMapType::Reinhard;

		NE_REFLECT_BEGIN(BloomSettings)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(tint),
			NE_REFLECT_FIELD(brightThreshold),
			NE_REFLECT_FIELD(brightScale),
			NE_REFLECT_FIELD(softKnee),
			NE_REFLECT_FIELD(bloomRadius),
			NE_REFLECT_FIELD(bloomIntensity),
			NE_REFLECT_FIELD(exposure)
		NE_REFLECT_END()
	};

	struct SSAOSettings {
		bool enabled = false;

		float radius = 0.5f;
		float bias = 0.025f;
		float intensity = 1.0f;
		float power = 1.5f;

		NE_REFLECT_BEGIN(SSAOSettings)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(radius),
			NE_REFLECT_FIELD(bias),
			NE_REFLECT_FIELD(intensity),
			NE_REFLECT_FIELD(power)
		NE_REFLECT_END()
	};

	struct TAASettings {
		bool enabled = false;
		float feedbackMin = 0.05f;
		float feedbackMax = 0.95f;
		float sharpen = 0.0f;
		bool resetHistory = false;

		NE_REFLECT_BEGIN(TAASettings)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(feedbackMin),
			NE_REFLECT_FIELD(feedbackMax),
			NE_REFLECT_FIELD(sharpen),
			NE_REFLECT_FIELD(resetHistory)
		NE_REFLECT_END()
	};

	struct SSRSettings {
		bool enabled = false;
		bool temporalEnabled = true;
		float intensity = 0.6f;
		float halfResScale = 0.5f;
		int resolveTapCount = 4;
		float maxDistance = 30.0f;
		float thickness = 0.0f;
		int maxSteps = 48;
		float stride = 0.2f;
		int binarySearchSteps = 5;
		float roughnessCutoff = 0.85f;
		float fresnelPower = 5.0f;
		float edgeFade = 0.2f;
		bool hizEnabled = true;
		int hizStartMip = 4;
		int hizRefineSteps = 10;
		float hizDepthBias = 0.0005f;
		float temporalHistoryMin = 0.65f;
		float temporalHistoryMax = 0.92f;
		float temporalNormalRejectDot = 0.9f;
		float temporalRoughnessReject = 0.2f;
		float temporalDepthReject = 0.003f;
		float temporalProjectionResetThreshold = 0.05f;

		NE_REFLECT_BEGIN(SSRSettings)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(temporalEnabled),
			NE_REFLECT_FIELD(intensity),
			NE_REFLECT_FIELD(halfResScale),
			NE_REFLECT_FIELD(resolveTapCount),
			NE_REFLECT_FIELD(maxDistance),
			NE_REFLECT_FIELD(thickness),
			NE_REFLECT_FIELD(maxSteps),
			NE_REFLECT_FIELD(stride),
			NE_REFLECT_FIELD(binarySearchSteps),
			NE_REFLECT_FIELD(roughnessCutoff),
			NE_REFLECT_FIELD(fresnelPower),
			NE_REFLECT_FIELD(edgeFade),
			NE_REFLECT_FIELD(hizEnabled),
			NE_REFLECT_FIELD(hizStartMip),
			NE_REFLECT_FIELD(hizRefineSteps),
			NE_REFLECT_FIELD(hizDepthBias),
			NE_REFLECT_FIELD(temporalHistoryMin),
			NE_REFLECT_FIELD(temporalHistoryMax),
			NE_REFLECT_FIELD(temporalNormalRejectDot),
			NE_REFLECT_FIELD(temporalRoughnessReject),
			NE_REFLECT_FIELD(temporalDepthReject),
			NE_REFLECT_FIELD(temporalProjectionResetThreshold)
		NE_REFLECT_END()
	};

	struct VignetteSettings {
		bool enabled = false;
		float intensity = 0.25f;
		float radius = 0.95f;
		float softness = 0.35f;
		Math::Vec3 tint{ 1.f, 1.f, 1.f };
		float tintIntensity = 0.f;

		NE_REFLECT_BEGIN(VignetteSettings)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(intensity),
			NE_REFLECT_FIELD(radius),
			NE_REFLECT_FIELD(softness),
			NE_REFLECT_FIELD(tint),
			NE_REFLECT_FIELD(tintIntensity)
			NE_REFLECT_END()
	};

	struct PostProcessingSettings {
		bool enabled = true;
		BloomSettings bloomSettings;
		SSAOSettings  ssaoSettings;
		TAASettings taaSettings;
		SSRSettings ssrSettings;
		VignetteSettings vignetteSettings;

		NE_REFLECT_BEGIN(PostProcessingSettings)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(bloomSettings),
			NE_REFLECT_FIELD(ssaoSettings),
			NE_REFLECT_FIELD(taaSettings),
			NE_REFLECT_FIELD(ssrSettings),
			NE_REFLECT_FIELD(vignetteSettings)
		NE_REFLECT_END()
	};

}
