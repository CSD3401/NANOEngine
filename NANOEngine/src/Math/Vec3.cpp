#include "Vec3.hpp"
#include "Vec2.hpp"
#include "Vec4.hpp"
#include "../../include/ScriptSDK/ScriptTypes.h"

namespace NE::Math {
	constexpr float EPSILON = 1e-6f;

	Vec3::Vec3(float x, float y, float z) noexcept
		: x(x), y(y), z(z)
	{
	}

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

#pragma region Arithmetic Operators
	Vec3 Vec3::operator+(const Vec3& rhs) const noexcept
	{
		return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
	}

	Vec3& Vec3::operator+=(const Vec3& rhs) noexcept
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	Vec3 Vec3::operator-(const Vec3& rhs) const noexcept
	{
		return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
	}

	Vec3& Vec3::operator-=(const Vec3& rhs) noexcept
	{
		x -= rhs.x; y -= rhs.y; z -= rhs.z;
		return *this;
	}

	Vec3 Vec3::operator-() const noexcept
	{
		return Vec3(-x, -y, -z);
	}

	Vec3 Vec3::operator*(float scalar) const noexcept
	{
		return Vec3(scalar * x, scalar * y, scalar * z);
	}

	Vec3& Vec3::operator*=(float scalar) noexcept
	{
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}

	Vec3 Vec3::operator*(const Vec3& rhs) const noexcept
	{
		return Vec3(x * rhs.x, y * rhs.y, z * rhs.z);
	}

	Vec3& Vec3::operator*=(const Vec3& rhs) noexcept
	{
		x *= rhs.x;
		y *= rhs.y;
		z *= rhs.z;
		return *this;
	}

	Vec3 Vec3::operator/(float scalar) const
	{
		if (std::abs(scalar) <= EPSILON) {
			throw std::runtime_error("Division by zero");
		}
		return Vec3(x / scalar, y / scalar, z / scalar);
	}

	Vec3& Vec3::operator/=(float scalar)
	{
		if (std::abs(scalar) <= EPSILON) {
			throw std::runtime_error("Division by zero");
		}
		x /= scalar;
		y /= scalar;
		z /= scalar;
		return *this;
	}

	Vec3 Vec3::operator/(const Vec3& rhs) const noexcept
	{
		return Vec3(x / rhs.x, y / rhs.y, z / rhs.z);
	}

	Vec3& Vec3::operator/=(const Vec3& rhs) noexcept
	{
		x /= rhs.x;
		y /= rhs.y;
		z /= rhs.z;
		return *this;
	}

	bool Vec3::operator==(const Vec3& rhs) const noexcept
	{
		return std::fabs(x - rhs.x) <= EPSILON
			&& std::fabs(y - rhs.y) <= EPSILON
			&& std::fabs(z - rhs.z) <= EPSILON;
	}

	bool Vec3::operator!=(const Vec3& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	Vec3& Vec3::operator=(const Vec3& rhs) noexcept
	{
		x = rhs.x;
		y = rhs.y;
		z = rhs.z;
		return *this;
	}
#pragma endregion

	Vec3::operator Vec2() const {
		return Vec2(x, y);
	}

	//Vec3::operator Vec4() const {
	//	return Vec4(x, y, z, 0.0f);
	//}

	float Vec3::Length() const
	{
		return sqrt(x * x + y * y + z * z);
	}

	float Vec3::LengthSquared() const
	{
		return x * x + y * y + z * z;
	}

	float Vec3::Dot(const Vec3& rhs) const
	{
		return x * rhs.x + y * rhs.y + z * rhs.z;
	}

	Vec3 Vec3::Cross(const Vec3& rhs) const
	{
		return Vec3(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
	}

	Vec3 Vec3::Normalized() const
	{
		float d = Length();
		if (std::abs(d) <= EPSILON) {
			return *this;
		}
		return Vec3(*this / d);
	}

	Vec3& Vec3::Normalize()
	{
		float d = Length();
		if (std::abs(d) <= EPSILON) {
			return *this;
		}
		*this /= d;
		return *this;
	}

	std::ostream& operator<<(std::ostream& os, const Vec3& rhs) {
		os << "[ " << rhs.x << ", " << rhs.y << ", " << rhs.z << " ]";
		return os;
	}

	Vec3 operator*(float scalar, const Vec3& rhs)
	{
		return rhs * scalar;
	}

	Vec3 operator/(float scalar, const Vec3& rhs)
	{
		return Vec3(scalar / rhs.x,
			scalar / rhs.y,
			scalar / rhs.z);
	}
}