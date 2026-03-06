#pragma once

#include "Core/Reflection.hpp"
#include "Math/Vec4.hpp"

namespace NE::Graphics {
	struct SelectionHighlightSettings {
		bool enabled = true;
		Math::Vec4 outlineColor{ 0.995f, 0.43f, 0.034f, 1.0f };
		float outlineThicknessPx = 4.0f;
		float outlineOpacity = 0.95f;
		float outlineSoftness = 1.0f;
		bool fillEnabled = false;
		Math::Vec4 fillColor{ 0.23f, 0.62f, 0.96f, 1.0f };
		float fillIntensity = 0.3f;
		bool debugShowMask = false;
		bool debugOutlineOnly = false;

		NE_REFLECT_BEGIN(SelectionHighlightSettings)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(outlineColor),
			NE_REFLECT_FIELD(outlineThicknessPx),
			NE_REFLECT_FIELD(outlineOpacity),
			NE_REFLECT_FIELD(outlineSoftness),
			NE_REFLECT_FIELD(fillEnabled),
			NE_REFLECT_FIELD(fillColor),
			NE_REFLECT_FIELD(fillIntensity),
			NE_REFLECT_FIELD(debugShowMask),
			NE_REFLECT_FIELD(debugOutlineOnly)
		NE_REFLECT_END()
	};
}
