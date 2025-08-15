#include "Vec2.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"

namespace NE::Math {
	Vec2::Vec2(float a, float b) noexcept : x(a), y(b)
	{
	}

	Vec2 Vec2::operator+(const Vec2& rhs) const noexcept
	{
		return Vec2(x + rhs.x, y + rhs.y);
	}

	Vec2& Vec2::operator+=(const Vec2& rhs) noexcept
	{
		x += rhs.x;
		y += rhs.y;
		return *this;
	}

	Vec2 Vec2::operator-(const Vec2& rhs) const noexcept
	{
		return Vec2(x - rhs.x, y - rhs.y);
	}

	Vec2 Vec2::operator-=(const Vec2& rhs) noexcept
	{
		x -= rhs.x;
		y -= rhs.y;
		return *this;
	}

	Vec2 Vec2::operator-() const noexcept
	{
		return Vec2(-x, -y);
	}

	Vec2 Vec2::operator*(float scalar) const noexcept
	{
		return Vec2(scalar * x, scalar * y);
	}

	Vec2& Vec2::operator*=(float scalar) noexcept
	{
		x *= scalar;
		y *= scalar;
		return *this;
	}

	Vec2 Vec2::operator*(const Vec2& rhs) const noexcept
	{
		return Vec2(x * rhs.x, y * rhs.y);
	}

	Vec2& Vec2::operator*=(const Vec2& rhs) noexcept
	{
		x *= rhs.x;
		y *= rhs.y;
		return *this;
	}

	Vec2 Vec2::operator/(float scalar) const
	{
		return Vec2(x / scalar, y / scalar);
	}

	Vec2& Vec2::operator/=(float scalar)
	{
		x /= scalar;
		y /= scalar;
		return *this;
	}

	bool Vec2::operator==(const Vec2& rhs) const noexcept
	{
		return (x == rhs.x) && (y == rhs.y);
	}

	bool Vec2::operator!=(const Vec2& rhs) const noexcept
	{
		return !(*this == rhs);
	}

	Vec2& Vec2::operator=(const Vec2& rhs) noexcept
	{
		x = rhs.x;
		y = rhs.y;
		return *this;
	}

	Vec2::operator Vec3() const
	{
		return Vec3(x, y, 0.0f);
	}

	//Vec2::operator Vec4() const
	//{
	//	return Vec4(x, y, 0.0f, 0.0f);
	//}

	float Vec2::Length() const
	{
		return sqrt(x * x + y * y);
	}

	float Vec2::LengthSquared() const
	{
		return x * x + y * y;
	}

	float Vec2::Dot(const Vec2& rhs) const
	{
		return x * rhs.x + y * rhs.y;
	}

	float Vec2::Cross(const Vec2& rhs) const
	{
		return x * rhs.y - y * rhs.x;
	}

	Vec2 Vec2::Normalized() const
	{
		float d = Length();
		if (d <= FLT_EPSILON && -d <= FLT_EPSILON) {
			return *this;
		}
		return Vec2(*this / d);
	}

	Vec2& Vec2::Normalize()
	{
		float d = Length();
		if (d <= FLT_EPSILON && -d <= FLT_EPSILON) {
			return *this;
		}
		*this /= d;
		return *this;
	}

	std::ostream& operator<<(std::ostream& os, const Vec2& rhs) {
		os << "[ " << rhs.x << ", " << rhs.y << ", " << " ]";
		return os;
	}

	Vec2 operator*(float scalar, const Vec2& rhs)
	{
		return rhs * scalar;
	}

	Vec2 operator/(float scalar, const Vec2& rhs)
	{
		return rhs / scalar;
	}
}