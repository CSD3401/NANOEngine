#pragma once

#include "Core/Reflection.hpp"

namespace NE::ECS::Component {
	struct CharacterController {
		uint64_t luid = 0;

		float maxSlopeAngleDeg = 45.0f;
		float maxStrength = 1000.0f;
		float characterPadding = 0.02f;
		float penRecoverySpeed = 1.0f;
		float predictiveContactDistance = 0.1f;
		float supportingVolumeDepth = 1.0f;

		NE_REFLECT_BEGIN(CharacterController)
			NE_REFLECT_FIELD_NAMED(maxSlopeAngleDeg, "Max Slope Angle"),
			NE_REFLECT_FIELD_NAMED(maxStrength, "Max Strength"),
			NE_REFLECT_FIELD_NAMED(characterPadding, "Character Padding"),
			NE_REFLECT_FIELD_NAMED(penRecoverySpeed, "Penetration Recovery Speed"),
			NE_REFLECT_FIELD_NAMED(predictiveContactDistance, "Predictive Contact Distance"),
			NE_REFLECT_FIELD_NAMED(supportingVolumeDepth, "Supporting Volume Depth"),
			NE_REFLECT_FIELD_HIDDEN(luid)
			NE_REFLECT_END()
	};
}