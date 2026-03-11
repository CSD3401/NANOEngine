#ifndef VEC_3_HPP
#define VEC_3_HPP

#include <ostream>
#include "../NANOEngineAPI.hpp"

// Forward declaration of Scripting::Vec3 for implicit conversion
namespace NE { namespace Scripting { struct Vec3; } }

namespace NE::Math {
	struct Vec2;
	struct Vec4;

	inline constexpr float EPSILON = 1e-6f;

	struct NANOENGINE_API Vec3 {
		float x, y, z;

		constexpr inline Vec3(float x = 0, float y = 0, float z = 0) noexcept
			: x(x), y(y), z(z) { }
		Vec3(const Vec3& rhs) = default;
		Vec3(Vec3&&) = default;
		Vec3& operator=(Vec3&&) = default;
		~Vec3() = default;

		// Implicit conversion from Scripting::Vec3
		Vec3(const NE::Scripting::Vec3& other) noexcept;

		// Implicit conversion to Scripting::Vec3
		operator NE::Scripting::Vec3() const noexcept;

		// Assignment operator for Scripting::Vec3
		Vec3& operator=(const NE::Scripting::Vec3& other) noexcept;

#pragma region Arithmetic Operators
		inline Vec3 operator+(const Vec3& rhs) const noexcept {
			return Vec3(x + rhs.x, y + rhs.y, z + rhs.z);
		}

		inline Vec3& operator+=(const Vec3& rhs) noexcept {
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;
			return *this;
		}

		inline Vec3 operator-(const Vec3& rhs) const noexcept {
			return Vec3(x - rhs.x, y - rhs.y, z - rhs.z);
		}

		inline Vec3& operator-=(const Vec3& rhs) noexcept {
			x -= rhs.x; y -= rhs.y; z -= rhs.z;
			return *this;
		}

		inline Vec3 operator-() const noexcept {
			return Vec3(-x, -y, -z);
		}

		inline Vec3 operator*(float scalar) const noexcept {
			return Vec3(scalar * x, scalar * y, scalar * z);
		}

		inline Vec3& operator*=(float scalar) noexcept {
			x *= scalar;
			y *= scalar;
			z *= scalar;
			return *this;
		}

		inline Vec3 operator*(const Vec3& rhs) const noexcept {
			return Vec3(x * rhs.x, y * rhs.y, z * rhs.z);
		}

		inline Vec3& operator*=(const Vec3& rhs) noexcept {
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;
			return *this;
		}

		inline Vec3 operator/(float scalar) const {
			if (std::abs(scalar) <= EPSILON) {
				throw std::runtime_error("Division by zero");
			}
			return Vec3(x / scalar, y / scalar, z / scalar);
		}

		inline Vec3& operator/=(float scalar) {
			if (std::abs(scalar) <= EPSILON) {
				throw std::runtime_error("Division by zero");
			}
			x /= scalar;
			y /= scalar;
			z /= scalar;
			return *this;
		}

		inline Vec3 operator/(const Vec3& rhs) const noexcept {
			return Vec3(x / rhs.x, y / rhs.y, z / rhs.z);
		}

		inline Vec3& operator/=(const Vec3& rhs) noexcept {
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;
			return *this;
		}

		inline bool operator==(const Vec3& rhs) const noexcept {
			return std::fabs(x - rhs.x) <= EPSILON
				&& std::fabs(y - rhs.y) <= EPSILON
				&& std::fabs(z - rhs.z) <= EPSILON;
		}

		inline bool operator!=(const Vec3& rhs) const noexcept {
			return !(*this == rhs);
		}

		inline Vec3& operator=(const Vec3& rhs) noexcept {
			x = rhs.x;
			y = rhs.y;
			z = rhs.z;
			return *this;
		}
#pragma endregion

		//inline explicit operator Vec2() const {
		//	return Vec2(x, y);
		//}

		//inline explicit operator Vec4() const {
		//		return Vec4(x, y, z, 0.0f);
		//}

		inline float Length() const {
			return sqrt(x * x + y * y + z * z);
		}

		inline float LengthSquared() const {
			return x * x + y * y + z * z;
		}

		inline float Dot(const Vec3& rhs) const {
			return x * rhs.x + y * rhs.y + z * rhs.z;
		}

		inline Vec3 Cross(const Vec3& rhs) const {
			return Vec3(y * rhs.z - z * rhs.y, z * rhs.x - x * rhs.z, x * rhs.y - y * rhs.x);
		}

		inline Vec3 Normalized() const {
			float d = Length();
			if (std::abs(d) <= EPSILON) {
				return *this;
			}
			return Vec3(*this / d);
		}

		inline Vec3& Normalize() {
			float d = Length();
			if (std::abs(d) <= EPSILON) {
				return *this;
			}
			*this /= d;
			return *this;
		}

		inline bool Zero() const {
			return x == 0 && y == 0 && z == 0;
		}

		inline friend std::ostream& operator<<(std::ostream& os, const Vec3& rhs) {
			os << "[ " << rhs.x << ", " << rhs.y << ", " << rhs.z << " ]";
			return os;
		}

		inline friend Vec3 operator*(float scalar, const Vec3& rhs) {
			return rhs * scalar;
		}

		inline friend Vec3 operator/(float scalar, const Vec3& rhs) {
			return Vec3(scalar / rhs.x,
				scalar / rhs.y,
				scalar / rhs.z);
		}
		
		inline float* Data() { return &x; }
		inline const float* Data() const { return &x; }
	};
}

#endif // !VECTOR_3_HPP