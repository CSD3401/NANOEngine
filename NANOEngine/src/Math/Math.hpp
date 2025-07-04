//#ifndef MATH_HPP
//#define MATH_HPP
//
//#include "Mat3.hpp"
//#include "Mat4.hpp"
//
//namespace Math {
//
//}
//
//inline Vec4 Mat4::operator*(const Vec4& rhs) {
//	Vec4 vec4;
//	for (unsigned int i = 0; i < 4; ++i) {
//		vec4[i] = GetElement(i, 0) * rhs[0] +
//			GetElement(i, 1) * rhs[1] +
//			GetElement(i, 2) * rhs[2] +
//			GetElement(i, 3) * rhs[3];
//	}
//	return vec4;
//}
//
//inline Vec3 Mat4::operator*(const Vec3& rhs) {
//	Vec3 vec3;
//	for (unsigned int i = 0; i < 3; ++i) {
//		vec3[i] = GetElement(i, 0) * rhs.x +
//			GetElement(i, 1) * rhs.y +
//			GetElement(i, 2) * rhs.z +
//			GetElement(i, 3) * 1.f;
//	}
//	return vec3;
//}
//
//inline Vec3 Mat4::GetRow3(unsigned int row) const {
//	return Vec3(GetElement(row, 0), GetElement(row, 1), GetElement(row, 2));
//}
//
//
//inline Vec4 Mat4::GetRow4(unsigned int row) const {
//	return Vec4(GetElement(row, 0), GetElement(row, 1), GetElement(row, 2), GetElement(row, 3));
//}
//
//inline Vec3 Mat4::GetCol3(unsigned int col) const {
//	return Vec3(GetElement(0, col), GetElement(1, col), GetElement(2, col));
//}
//
//inline Vec4 Mat4::GetCol4(unsigned int col) const {
//	return Vec4(GetElement(0, col), GetElement(1, col), GetElement(2, col), GetElement(3, col));
//}
//
//inline float Mat4::Cofactor(unsigned int row, unsigned int col) const {
//	// Create a 3x3 submatrix
//	Mat3 subMatrix = CreateSubMat3(row, col);
//	return subMatrix.Determinant();
//}
//
//inline Mat3 Mat4::CreateSubMat3(unsigned int row, unsigned int col) const {
//	Mat3 result;
//	unsigned int subRow = 0, subCol = 0;
//
//	for (unsigned int i = 0; i < 4; ++i) {
//		if (i == row) continue;  // Skip the specified row
//		subCol = 0;
//		for (unsigned int j = 0; j < 4; ++j) {
//			if (j == col) continue;  // Skip the specified column
//			result.GetElement(subRow, subCol) = GetElement(i, j);
//			++subCol;
//		}
//		++subRow;
//	}
//	return result;
//}
//
//inline Mat4 Mat4::BuildTranslation(const Vec3& vec3) {
//	return BuildTranslation(vec3.x, vec3.y, vec3.z);
//}
//
//inline Mat4 Mat4::BuildRotation(float degrees, float x, float y, float z) {
//	Vec3 n{ x, y, z };
//	n.Normalize();
//
//	float rad = degrees * (PI / 180.f);
//	float cos_d = cosf(rad);
//	float one_minus_cos_d = 1.f - cos_d;
//	float sin_d = sinf(rad);
//
//	Mat4 mRot{ cos_d + (one_minus_cos_d * (n.x * n.x)),		   (one_minus_cos_d * (n.x * n.y)) + (sin_d * -n.z), (one_minus_cos_d * (n.x * n.z)) + (sin_d * n.y),  0.f,
//			  (one_minus_cos_d * (n.x * n.y)) + (sin_d * n.z),  cos_d + (one_minus_cos_d * (n.y * n.y)),		 (one_minus_cos_d * (n.y * n.z)) + (sin_d * -n.x), 0.f,
//			  (one_minus_cos_d * (n.x * n.z)) + (sin_d * -n.y), (one_minus_cos_d * (n.y * n.z)) + (sin_d * n.x),  cos_d + (one_minus_cos_d * (n.z * n.z)),         0.f,
//			  0.f,												 0.f,											  0.f,											   1.f };
//
//	return mRot;
//}
//
//inline Mat4 Mat4::BuildRotation(float degrees, const Vec3& axis) {
//	return BuildRotation(degrees, axis.x, axis.y, axis.z);
//}
//
//
//inline Mat4 Mat4::BuildScaling(const Vec3& pivot, const Vec3& scaleFactors) {
//	return BuildScaling(pivot.x, pivot.y, pivot.z, scaleFactors.x, scaleFactors.y, scaleFactors.z);
//}
//
//inline Mat4 Mat4::BuildViewMtx(const Vec3& eye, const Vec3& tgt, const Vec3& up) {
//	Vec3 w = eye - tgt;
//	w.Normalize();
//	Vec3 u = up.Cross(w);
//	u.Normalize();
//	Vec3 v = w.Cross(u);
//
//	Mat4 mView{ u.x, u.y, u.z, -u.Dot(eye),
//				v.x, v.y, v.z, -v.Dot(eye),
//				w.x, w.y, w.z, -w.Dot(eye),
//				  0,   0,   0,   1 };
//
//	return mView;
//}
//
//#endif // !MATH_HPP
