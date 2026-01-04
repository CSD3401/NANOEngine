#pragma once

#include "Math/Vec3.hpp"

namespace NE::Physics {
	struct Ray {
		Math::Vec3 direction{ 0.f, 0.f, 0.f };
		Math::Vec3 origin{ 0.f, 0.f, 0.f };
	};
}
