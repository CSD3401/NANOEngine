#ifndef QUAT_HPP
#define QUAT_HPP

#include "../NANOEngineAPI.hpp"
#include <ostream>

namespace NE::Math {
	struct Vec3;
	struct Mat4;

	/*!***********************************************************************
	\brief
		Quaternion struct for representing rotations.
		Stored as (x, y, z, w) where w is the scalar component.
		A unit quaternion represents a rotation: q = cos(θ/2) + sin(θ/2)(xi + yj + zk)
	*************************************************************************/
	struct NANOENGINE_API Quat {
		float x, y, z, w;

		/*!***********************************************************************
		\brief
			Default constructor. Creates an identity quaternion (no rotation).
		*************************************************************************/
		Quat() noexcept;

		/*!***********************************************************************
		\brief
			Constructs a quaternion with specified components.
		\param[in] float x, float y, float z
			Imaginary components.
		\param[in] float w
			Scalar (real) component.
		*************************************************************************/
		Quat(float x, float y, float z, float w) noexcept;

		Quat(const Quat& rhs) = default;
		Quat(Quat&&) = default;
		Quat& operator=(Quat&&) = default;
		Quat& operator=(const Quat& rhs) = default;
		~Quat() = default;

#pragma region Arithmetic Operators
		/*!***********************************************************************
		\brief
			Quaternion multiplication (composition of rotations).
			Order matters: (a * b) applies rotation b first, then a.
		\param[in] const Quat& rhs
			Right-hand side quaternion.
		\return
			Composed quaternion.
		*************************************************************************/
		Quat operator*(const Quat& rhs) const noexcept;
		Quat& operator*=(const Quat& rhs) noexcept;

		/*!***********************************************************************
		\brief
			Scalar multiplication.
		\param[in] float scalar
			Scalar value.
		\return
			Scaled quaternion.
		*************************************************************************/
		Quat operator*(float scalar) const noexcept;
		Quat& operator*=(float scalar) noexcept;

		/*!***********************************************************************
		\brief
			Quaternion addition.
		\param[in] const Quat& rhs
			Right-hand side quaternion.
		\return
			Sum of quaternions.
		*************************************************************************/
		Quat operator+(const Quat& rhs) const noexcept;
		Quat& operator+=(const Quat& rhs) noexcept;

		/*!***********************************************************************
		\brief
			Quaternion subtraction.
		\param[in] const Quat& rhs
			Right-hand side quaternion.
		\return
			Difference of quaternions.
		*************************************************************************/
		Quat operator-(const Quat& rhs) const noexcept;
		Quat& operator-=(const Quat& rhs) noexcept;

		/*!***********************************************************************
		\brief
			Negation operator.
		\return
			Negated quaternion.
		*************************************************************************/
		Quat operator-() const noexcept;

		bool operator==(const Quat& rhs) const noexcept;
		bool operator!=(const Quat& rhs) const noexcept;
#pragma endregion

		/*!***********************************************************************
		\brief
			Returns the length (magnitude) of the quaternion.
		\return
			Length of the quaternion.
		*************************************************************************/
		float Length() const;

		/*!***********************************************************************
		\brief
			Returns the squared length of the quaternion.
		\return
			Squared length of the quaternion.
		*************************************************************************/
		float LengthSquared() const noexcept;

		/*!***********************************************************************
		\brief
			Returns the dot product of two quaternions.
		\param[in] const Quat& rhs
			Right-hand side quaternion.
		\return
			Dot product.
		*************************************************************************/
		float Dot(const Quat& rhs) const noexcept;

		/*!***********************************************************************
		\brief
			Returns a normalized copy of this quaternion.
		\return
			Normalized quaternion.
		*************************************************************************/
		Quat Normalized() const;

		/*!***********************************************************************
		\brief
			Normalizes this quaternion in place.
		\return
			Reference to this quaternion.
		*************************************************************************/
		Quat& Normalize();

		/*!***********************************************************************
		\brief
			Returns the conjugate of this quaternion.
			For unit quaternions, conjugate equals inverse.
		\return
			Conjugate quaternion.
		*************************************************************************/
		Quat Conjugate() const noexcept;

		/*!***********************************************************************
		\brief
			Returns the inverse of this quaternion.
		\return
			Inverse quaternion.
		*************************************************************************/
		Quat Inverse() const;

		/*!***********************************************************************
		\brief
			Converts this quaternion to a 4x4 rotation matrix.
		\return
			Rotation matrix.
		*************************************************************************/
		Mat4 ToMat4() const;

		/*!***********************************************************************
		\brief
			Converts this quaternion to Euler angles in degrees.
			Returns Vec3(pitch, yaw, roll) where:
			- pitch = rotation around X axis
			- yaw = rotation around Y axis
			- roll = rotation around Z axis
		\return
			Euler angles in degrees.
		*************************************************************************/
		Vec3 ToEulerDegrees() const;

		/*!***********************************************************************
		\brief
			Converts this quaternion to Euler angles in radians.
		\return
			Euler angles in radians.
		*************************************************************************/
		Vec3 ToEulerRadians() const;

		/*!***********************************************************************
		\brief
			Rotates a vector by this quaternion.
		\param[in] const Vec3& v
			Vector to rotate.
		\return
			Rotated vector.
		*************************************************************************/
		Vec3 RotateVector(const Vec3& v) const;

		/*!***********************************************************************
		\brief
			Creates a quaternion from Euler angles in degrees.
		\param[in] float pitch
			Rotation around X axis in degrees.
		\param[in] float yaw
			Rotation around Y axis in degrees.
		\param[in] float roll
			Rotation around Z axis in degrees.
		\return
			Quaternion representing the rotation.
		*************************************************************************/
		static Quat FromEulerDegrees(float pitch, float yaw, float roll);

		/*!***********************************************************************
		\brief
			Creates a quaternion from Euler angles in degrees.
		\param[in] const Vec3& euler
			Vec3 containing (pitch, yaw, roll) in degrees.
		\return
			Quaternion representing the rotation.
		*************************************************************************/
		static Quat FromEulerDegrees(const Vec3& euler);

		/*!***********************************************************************
		\brief
			Creates a quaternion from Euler angles in radians.
		\param[in] float pitch
			Rotation around X axis in radians.
		\param[in] float yaw
			Rotation around Y axis in radians.
		\param[in] float roll
			Rotation around Z axis in radians.
		\return
			Quaternion representing the rotation.
		*************************************************************************/
		static Quat FromEulerRadians(float pitch, float yaw, float roll);

		/*!***********************************************************************
		\brief
			Creates a quaternion from an axis and angle.
		\param[in] const Vec3& axis
			Normalized axis of rotation.
		\param[in] float angleDegrees
			Angle of rotation in degrees.
		\return
			Quaternion representing the rotation.
		*************************************************************************/
		static Quat FromAxisAngle(const Vec3& axis, float angleDegrees);

		/*!***********************************************************************
		\brief
			Spherical linear interpolation between two quaternions.
			Provides smooth rotation interpolation.
		\param[in] const Quat& a
			Start quaternion.
		\param[in] const Quat& b
			End quaternion.
		\param[in] float t
			Interpolation factor [0, 1].
		\return
			Interpolated quaternion.
		*************************************************************************/
		static Quat Slerp(const Quat& a, const Quat& b, float t);

		/*!***********************************************************************
		\brief
			Linear interpolation between two quaternions (faster but less accurate).
		\param[in] const Quat& a
			Start quaternion.
		\param[in] const Quat& b
			End quaternion.
		\param[in] float t
			Interpolation factor [0, 1].
		\return
			Interpolated quaternion (normalized).
		*************************************************************************/
		static Quat Lerp(const Quat& a, const Quat& b, float t);

		/*!***********************************************************************
		\brief
			Returns an identity quaternion (no rotation).
		\return
			Identity quaternion (0, 0, 0, 1).
		*************************************************************************/
		static Quat Identity() noexcept;

		float* Data() { return &x; }
		const float* Data() const { return &x; }

		friend std::ostream& operator<<(std::ostream& os, const Quat& q);
		friend Quat operator*(float scalar, const Quat& q);
	};
}

#endif // !QUAT_HPP
