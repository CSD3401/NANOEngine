#include "pch.h"
#include "Vec3.hpp"
#include "Vec2.hpp"
#include "Vec4.hpp"
#include "../../include/ScriptSDK/ScriptTypes.h"

namespace NE::Math {
	// Conversion from Scripting::Vec3
	Vec3::Vec3(const NE::Scripting::Vec3& other) noexcept
		: x(other.x), y(other.y), z(other.z)
	{
	}

	// Conversion to Scripting::Vec3
	Vec3::operator NE::Scripting::Vec3() const noexcept {
		return NE::Scripting::Vec3(x, y, z);
	}

	// Assignment operator for Scripting::Vec3
	Vec3& Vec3::operator=(const NE::Scripting::Vec3& other) noexcept {
		x = other.x;
		y = other.y;
		z = other.z;
		return *this;
	}
}