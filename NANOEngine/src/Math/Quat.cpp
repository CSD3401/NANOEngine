#include "Quat.hpp"
#include "Vec3.hpp"
#include "Mat4.hpp"
#include <cmath>

namespace NE::Math {
	namespace {
		Quat FromAxisAngleRadians(const Vec3& axis, float angleRad)
		{
			Vec3 n = axis.Normalized();
			float half = angleRad * 0.5f;
			float s = std::sin(half);
			return Quat(n.x * s, n.y * s, n.z * s, std::cos(half));
		}
	}

	constexpr float EPSILON = 1e-6f;

	Quat::Quat() noexcept
		: x(0.0f), y(0.0f), z(0.0f), w(1.0f)
	{
	}

	Quat::Quat(float x, float y, float z, float w) noexcept
		: x(x), y(y), z(z), w(w)
	{
	}

#pragma region Arithmetic Operators
	Quat Quat::operator*(const Quat& rhs) const noexcept
	{
		return Quat(
			w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
			w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
			w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w,
			w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z
		);
	}

	Quat& Quat::operator*=(const Quat& rhs) noexcept
	{
		*this = *this * rhs;
		return *this;
	}

	Quat Quat::operator*(float scalar) const noexcept
	{
		return Quat(x * scalar, y * scalar, z * scalar, w * scalar);
	}

	Quat& Quat::operator*=(float scalar) noexcept
	{
		x *= scalar;
		y *= scalar;
		z *= scalar;
		w *= scalar;
		return *this;
	}

	Quat Quat::operator+(const Quat& rhs) const noexcept
	{
		return Quat(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
	}

	Quat& Quat::operator+=(const Quat& rhs) noexcept
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		w += rhs.w;
		return *this;
	}

	Quat Quat::operator-(const Quat& rhs) const noexcept
	{
		return Quat(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
	}

	Quat& Quat::operator-=(const Quat& rhs) noexcept
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		w -= rhs.w;
		return *this;
	}

	Quat Quat::operator-() const noexcept
	{
		return Quat(-x, -y, -z, -w);
	}

	bool Quat::operator==(const Quat& rhs) const noexcept
	{
		return std::fabs(x - rhs.x) <= EPSILON
			&& std::fabs(y - rhs.y) <= EPSILON
			&& std::fabs(z - rhs.z) <= EPSILON
			&& std::fabs(w - rhs.w) <= EPSILON;
	}

	bool Quat::operator!=(const Quat& rhs) const noexcept
	{
		return !(*this == rhs);
	}
#pragma endregion

	float Quat::Length() const
	{
		return std::sqrt(x * x + y * y + z * z + w * w);
	}

	float Quat::LengthSquared() const noexcept
	{
		return x * x + y * y + z * z + w * w;
	}

	float Quat::Dot(const Quat& rhs) const noexcept
	{
		return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
	}

	Quat Quat::Normalized() const
	{
		float len = Length();
		if (std::abs(len) <= EPSILON) {
			return *this;
		}
		float invLen = 1.0f / len;
		return Quat(x * invLen, y * invLen, z * invLen, w * invLen);
	}

	Quat& Quat::Normalize()
	{
		float len = Length();
		if (std::abs(len) <= EPSILON) {
			return *this;
		}
		float invLen = 1.0f / len;
		x *= invLen;
		y *= invLen;
		z *= invLen;
		w *= invLen;
		return *this;
	}

	Quat Quat::Conjugate() const noexcept
	{
		return Quat(-x, -y, -z, w);
	}

	Quat Quat::Inverse() const
	{
		float lenSq = LengthSquared();
		if (std::abs(lenSq) <= EPSILON) {
			return *this;
		}
		float invLenSq = 1.0f / lenSq;
		return Quat(-x * invLenSq, -y * invLenSq, -z * invLenSq, w * invLenSq);
	}

	Quat Quat::InverseFast() const noexcept
	{
		// For unit quaternions, inverse == conjugate
		return Conjugate();
	}

	bool Quat::RotationEquals(const Quat& rhs) const noexcept
	{
		// q and -q represent the same rotation
		// Check if dot product is close to 1 or -1
		float dot = Dot(rhs);
		return std::fabs(std::fabs(dot) - 1.0f) <= EPSILON;
	}

	Mat4 Quat::ToMat4() const
	{
		// Assumes quaternion is normalized
		float xx = x * x;
		float yy = y * y;
		float zz = z * z;
		float xy = x * y;
		float xz = x * z;
		float yz = y * z;
		float wx = w * x;
		float wy = w * y;
		float wz = w * z;

		Mat4 result;
		result.SetToIdentity();

		// Row 0
		result.GetElement(0, 0) = 1.0f - 2.0f * (yy + zz);
		result.GetElement(0, 1) = 2.0f * (xy - wz);
		result.GetElement(0, 2) = 2.0f * (xz + wy);

		// Row 1
		result.GetElement(1, 0) = 2.0f * (xy + wz);
		result.GetElement(1, 1) = 1.0f - 2.0f * (xx + zz);
		result.GetElement(1, 2) = 2.0f * (yz - wx);

		// Row 2
		result.GetElement(2, 0) = 2.0f * (xz - wy);
		result.GetElement(2, 1) = 2.0f * (yz + wx);
		result.GetElement(2, 2) = 1.0f - 2.0f * (xx + yy);

		return result;
	}

	Vec3 Quat::ToEulerRadians() const
	{
		Vec3 euler;

		// Roll (X-axis rotation)
		float sinr_cosp = 2.0f * (w * x + y * z);
		float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
		euler.x = std::atan2(sinr_cosp, cosr_cosp);

		// Pitch (Y-axis rotation)
		float sinp = 2.0f * (w * y - z * x);
		if (std::abs(sinp) >= 1.0f) {
			// Use 90 degrees if out of range (gimbal lock)
			euler.y = std::copysign(PI * 0.5f, sinp);
		}
		else {
			euler.y = std::asin(sinp);
		}

		// Yaw (Z-axis rotation)
		float siny_cosp = 2.0f * (w * z + x * y);
		float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
		euler.z = std::atan2(siny_cosp, cosy_cosp);

		return euler;
	}

	Vec3 Quat::ToEulerDegrees() const
	{
		Vec3 radians = ToEulerRadians();
		return Vec3(radians.x * RAD_TO_DEG, radians.y * RAD_TO_DEG, radians.z * RAD_TO_DEG);
	}

	Vec3 Quat::RotateVector(const Vec3& v) const
	{
		// q * v * q^-1 (optimized version)
		Vec3 qv(x, y, z);
		Vec3 uv = qv.Cross(v);
		Vec3 uuv = qv.Cross(uv);
		return v + ((uv * w) + uuv) * 2.0f;
	}

	Quat Quat::FromEulerRadians(float pitchX, float yawY, float rollZ) {
		Quat qx = FromAxisAngleRadians(Vec3(1, 0, 0), pitchX);
		Quat qy = FromAxisAngleRadians(Vec3(0, 1, 0), yawY);
		Quat qz = FromAxisAngleRadians(Vec3(0, 0, 1), rollZ);

		return (qz * qx * qy).Normalized();
	}

	Quat Quat::FromEulerDegrees(float pitchX, float yawY, float rollZ) {
		return FromEulerRadians(pitchX * DEG_TO_RAD, yawY * DEG_TO_RAD, rollZ * DEG_TO_RAD);
	}

	Quat Quat::FromEulerDegrees(const Vec3& euler)
	{
		return FromEulerDegrees(euler.x, euler.y, euler.z);
	}

	Quat Quat::FromAxisAngle(const Vec3& axis, float angleDegrees)
	{
		Vec3 n = axis.Normalized();
		float angleRad = angleDegrees * DEG_TO_RAD;
		float halfAngle = angleRad * 0.5f;
		float s = std::sin(halfAngle);

		return Quat(
			n.x * s,
			n.y * s,
			n.z * s,
			std::cos(halfAngle)
		);
	}

	Quat Quat::Slerp(const Quat& a, const Quat& b, float t)
	{
		// Clamp t to [0, 1]
		if (t <= 0.0f) return a;
		if (t >= 1.0f) return b;

		Quat result;
		float dot = a.Dot(b);

		// If dot is negative, negate one quaternion to take shorter path
		Quat bAdjusted = b;
		if (dot < 0.0f) {
			bAdjusted = -b;
			dot = -dot;
		}

		// If quaternions are very close, use linear interpolation
		if (dot > 0.9995f) {
			result = Lerp(a, bAdjusted, t);
			return result;
		}

		// Clamp dot to valid range for acos
		dot = std::fmin(std::fmax(dot, -1.0f), 1.0f);

		float theta0 = std::acos(dot);
		float theta = theta0 * t;

		float sinTheta = std::sin(theta);
		float sinTheta0 = std::sin(theta0);

		float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
		float s1 = sinTheta / sinTheta0;

		result = a * s0 + bAdjusted * s1;
		return result;
	}

	Quat Quat::Lerp(const Quat& a, const Quat& b, float t)
	{
		// Clamp t to [0, 1]
		if (t <= 0.0f) return a;
		if (t >= 1.0f) return b;

		Quat result;

		// If dot is negative, negate one quaternion to take shorter path
		float dot = a.Dot(b);
		if (dot < 0.0f) {
			result = a * (1.0f - t) + (-b) * t;
		}
		else {
			result = a * (1.0f - t) + b * t;
		}

		return result.Normalized();
	}

	Quat Quat::Identity() noexcept
	{
		return Quat(0.0f, 0.0f, 0.0f, 1.0f);
	}

	std::ostream& operator<<(std::ostream& os, const Quat& q)
	{
		os << "[ " << q.x << ", " << q.y << ", " << q.z << ", " << q.w << " ]";
		return os;
	}

	Quat operator*(float scalar, const Quat& q)
	{
		return q * scalar;
	}
}
