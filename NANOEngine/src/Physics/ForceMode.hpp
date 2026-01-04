#pragma once
#include <cstdint>

namespace NE::Physics {
	enum class ForceMode : uint8_t {
		Force,
		//Acceleration,
		Impulse
		//VelocityChange
	};
}
