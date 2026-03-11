#ifndef VEC_2_HPP
#define VEC_2_HPP

#include <ostream>
#include <cmath>

#include "../NANOEngineAPI.hpp"

namespace NE::Math {
	struct Vec3;
	struct Vec4;

	struct NANOENGINE_API Vec2 {
		float x, y;

		Vec2(float a = 0, float b = 0) noexcept;
		Vec2(const Vec2& rhs) = default;
		Vec2(Vec2&&) = default;
		Vec2& operator=(Vec2&&) = default;
		~Vec2() = default;

		Vec2 operator+(const Vec2& rhs) const noexcept;
		Vec2& operator+=(const Vec2& rhs) noexcept;

		Vec2 operator-(const Vec2& rhs) const noexcept;
		Vec2 operator-=(const Vec2& rhs) noexcept;

		Vec2 operator-() const noexcept;

		Vec2 operator*(float scalar) const noexcept;
		Vec2& operator*=(float scalar) noexcept;
		Vec2 operator*(const Vec2& rhs) const noexcept;
		Vec2& operator*=(const Vec2& rhs) noexcept;

		Vec2 operator/(float scalar) const;
		Vec2& operator/=(float scalar);

		bool operator==(const Vec2& rhs) const noexcept;
		bool operator!=(const Vec2& rhs) const noexcept;

		Vec2& operator=(const Vec2& rhs) noexcept;

		explicit operator Vec3() const;
		//explicit operator Vec4() const;

		float Length() const;
		float LengthSquared() const;

		float Dot(const Vec2& rhs) const;
		float Cross(const Vec2& rhs) const;

		Vec2 Normalized() const; // Maybe add throw divide by zero error

		Vec2& Normalize(); // maybe add throw divide by zero error

		friend std::ostream& operator<<(std::ostream& os, const Vec2& rhs);
		friend Vec2 operator*(float scalar, const Vec2& rhs);
		friend Vec2 operator/(float scalar, const Vec2& rhs);
	};
}

#endif // !VEC_3_HPP