#ifndef VEC_3_HPP
#define VEC_3_HPP

#include "../NANOEngineAPI.hpp"
#include <ostream>

// Forward declaration of Scripting::Vec3 for implicit conversion
namespace NE { namespace Scripting { struct Vec3; } }

namespace NE::Math {
	struct Vec2;
	struct Vec4;

	struct NANOENGINE_API Vec3 {
		float x, y, z;

		Vec3(float x = 0, float y = 0, float z = 0) noexcept;
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
		Vec3 operator+(const Vec3& rhs) const noexcept;
		Vec3& operator+=(const Vec3& rhs) noexcept;

		Vec3 operator-(const Vec3& rhs) const noexcept;
		Vec3& operator-=(const Vec3& rhs) noexcept;

		Vec3 operator-() const noexcept;

		Vec3 operator*(float scalar) const noexcept;
		Vec3& operator*=(float scalar) noexcept;
		Vec3 operator*(const Vec3& rhs) const noexcept;
		Vec3& operator*=(const Vec3& rhs) noexcept;

		Vec3 operator/(float scalar) const;
		Vec3& operator/=(float scalar);
		Vec3 operator/(const Vec3& rhs) const noexcept;
		Vec3& operator/=(const Vec3& rhs) noexcept;

		bool operator==(const Vec3& rhs) const noexcept;
		bool operator!=(const Vec3& rhs) const noexcept;

		Vec3& operator=(const Vec3& rhs) noexcept;
#pragma endregion

		explicit operator Vec2() const;
		//explicit operator Vec4() const;

		float Length() const;
		float LengthSquared() const;

		float Dot(const Vec3& rhs) const;
		Vec3 Cross(const Vec3& rhs) const;

		Vec3 Normalized() const; // Maybe add throw divide by zero error

		Vec3& Normalize(); // maybe add throw divide by zero error

		friend std::ostream& operator<<(std::ostream& os, const Vec3& rhs);
		friend Vec3 operator*(float scalar, const Vec3& rhs);
		friend Vec3 operator/(float scalar, const Vec3& rhs);
		
		float* Data() { return &x; }
		const float* Data() const { return &x; }
	};
}

#endif // !VECTOR_3_HPP