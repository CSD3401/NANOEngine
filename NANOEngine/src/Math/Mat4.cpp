#include "Mat4.hpp"
#include "Mat3.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"

namespace NE::Math {
	Mat4::Mat4(const float& e00, const float& e01, const float& e02, const float& e03,
		const float& e10, const float& e11, const float& e12, const float& e13,
		const float& e20, const float& e21, const float& e22, const float& e23,
		const float& e30, const float& e31, const float& e32, const float& e33) {
		a[0] = e00, a[1] = e10, a[2] = e20, a[3] = e30;
		a[4] = e01, a[5] = e11, a[6] = e21, a[7] = e31;
		a[8] = e02, a[9] = e12, a[10] = e22, a[11] = e32;
		a[12] = e03, a[13] = e13, a[14] = e23, a[15] = e33;
	}

	Mat4::Mat4(const float arr[16]) {
		for (int i = 0; i < 16; ++i) {
			a[i] = arr[i];
		}
	}

	Mat4::Mat4(const Mat4& m) {
		a[0] = m.a[0], a[1] = m.a[1], a[2] = m.a[2], a[3] = m.a[3];
		a[4] = m.a[4], a[5] = m.a[5], a[6] = m.a[6], a[7] = m.a[7];
		a[8] = m.a[8], a[9] = m.a[9], a[10] = m.a[10], a[11] = m.a[11];
		a[12] = m.a[12], a[13] = m.a[13], a[14] = m.a[14], a[15] = m.a[15];
	}

	Mat4& Mat4::operator=(const Mat4& m) {
		a[0] = m.a[0], a[1] = m.a[1], a[2] = m.a[2], a[3] = m.a[3];
		a[4] = m.a[4], a[5] = m.a[5], a[6] = m.a[6], a[7] = m.a[7];
		a[8] = m.a[8], a[9] = m.a[9], a[10] = m.a[10], a[11] = m.a[11];
		a[12] = m.a[12], a[13] = m.a[13], a[14] = m.a[14], a[15] = m.a[15];

		return *this;
	}

	//Mat4 Mat4::operator*(const Mat4& rhs) {
	//	Mat4 mat4{};
	//	for (unsigned int i = 0; i < 4; ++i) {
	//		for (unsigned int j = 0; j < 4; ++j) {
	//			for (unsigned int k = 0; k < 4; ++k) {
	//				mat4.GetElement(i, j) += GetElement(i, k) * rhs.GetElement(k, j);
	//			}
	//		}
	//	}

	//	return mat4;
	//}

	Mat4 Mat4::operator*(float scalar) {
		Mat4 mat4;
		for (unsigned int i = 0; i < 16; ++i) {
			mat4[i] = (*this)[i] * scalar;
		}
		return mat4;
	}

	Mat4& Mat4::operator*=(const Mat4& rhs) {
		*this = *this * rhs;
		return *this;
	}

	void Mat4::SetToZero() {
		for (unsigned int i = 0; i < 16; ++i) {
			a[i] = 0.f;
		}
	}

	void Mat4::SetToIdentity() {
		SetToZero(); // First set all elements to zero
		GetElement(0, 0) = 1.0f;
		GetElement(1, 1) = 1.0f;
		GetElement(2, 2) = 1.0f;
		GetElement(3, 3) = 1.0f;
	}

	void Mat4::SetTo(const float& e00, const float& e01, const float& e02, const float& e03,
		const float& e10, const float& e11, const float& e12, const float& e13,
		const float& e20, const float& e21, const float& e22, const float& e23,
		const float& e30, const float& e31, const float& e32, const float& e33) {
		a[0] = e00, a[1] = e10, a[2] = e20, a[3] = e30;
		a[4] = e01, a[5] = e11, a[6] = e21, a[7] = e31;
		a[8] = e02, a[9] = e12, a[10] = e22, a[11] = e32;
		a[12] = e03, a[13] = e13, a[14] = e23, a[15] = e33;
	}

	float Mat4::Determinant() const {
		float det = 0.0f;

		// Cofactor expansion along the first row
		det += GetElement(0, 0) * Cofactor(0, 0);
		det -= GetElement(0, 1) * Cofactor(0, 1);
		det += GetElement(0, 2) * Cofactor(0, 2);
		det -= GetElement(0, 3) * Cofactor(0, 3);

		return det;
	}

	Mat4 Mat4::Transpose() const {
		Mat4 mat4;
		for (unsigned int i = 0; i < 4; ++i) {
			for (unsigned int j = 0; j < 4; ++j) {
				mat4.GetElement(i, j) = GetElement(j, i);
			}
		}
		return mat4;
	}

	Mat4& Mat4::TransposeInPlace() {
		for (unsigned int i = 0; i < 4; ++i) {
			for (unsigned int j = i + 1; j < 4; ++j) {
				std::swap(GetElement(i, j), GetElement(j, i));
			}
		}
		return *this;
	}

	bool Mat4::InverseInPlace() {
		float det = Determinant();
		if (det == 0.0f) {
			return false;  // Matrix is singular, cannot invert
		}

		Mat4 inverse = Inverse();
		*this = inverse;
		return true;
	}

	Mat4 Mat4::Inverse() const {
		Mat4 mInverse;
		float det = Determinant();
		if (det == 0.0f) {
			// Matrix is singular and cannot be inverted
			return *this;
		}

		float invDet = 1.0f / det;

		// Cofactors for row 0
		mInverse.GetElement(0, 0) = Cofactor(0, 0) * invDet;
		mInverse.GetElement(1, 0) = -Cofactor(0, 1) * invDet;
		mInverse.GetElement(2, 0) = Cofactor(0, 2) * invDet;
		mInverse.GetElement(3, 0) = -Cofactor(0, 3) * invDet;

		// Cofactors for row 1
		mInverse.GetElement(0, 1) = -Cofactor(1, 0) * invDet;
		mInverse.GetElement(1, 1) = Cofactor(1, 1) * invDet;
		mInverse.GetElement(2, 1) = -Cofactor(1, 2) * invDet;
		mInverse.GetElement(3, 1) = Cofactor(1, 3) * invDet;

		// Cofactors for row 2
		mInverse.GetElement(0, 2) = Cofactor(2, 0) * invDet;
		mInverse.GetElement(1, 2) = -Cofactor(2, 1) * invDet;
		mInverse.GetElement(2, 2) = Cofactor(2, 2) * invDet;
		mInverse.GetElement(3, 2) = -Cofactor(2, 3) * invDet;

		// Cofactors for row 3
		mInverse.GetElement(0, 3) = -Cofactor(3, 0) * invDet;
		mInverse.GetElement(1, 3) = Cofactor(3, 1) * invDet;
		mInverse.GetElement(2, 3) = -Cofactor(3, 2) * invDet;
		mInverse.GetElement(3, 3) = Cofactor(3, 3) * invDet;

		return mInverse;
	}

	Mat4 Mat4::BuildTranslation(float x, float y, float z) {
		Mat4 mTrans{};
		mTrans.SetToIdentity();
		mTrans.GetElement(0, 3) = x;
		mTrans.GetElement(1, 3) = y;
		mTrans.GetElement(2, 3) = z;
		return mTrans;
	}

	Mat4 Mat4::BuildZRotation(float degrees) {
		Mat4 mRot{};
		float rad = degrees * (PI / 180.f);
		mRot.SetToIdentity();
		mRot.GetElement(0, 0) = cosf(rad);
		mRot.GetElement(0, 1) = -sinf(rad);
		mRot.GetElement(1, 0) = sinf(rad);
		mRot.GetElement(1, 1) = cosf(rad);

		return mRot;
	}

	Mat4 Mat4::BuildXRotation(float degrees) {
		Mat4 mRot{};
		float rad = degrees * (PI / 180.f);
		mRot.SetToIdentity();
		mRot.GetElement(1, 1) = cosf(rad);
		mRot.GetElement(1, 2) = -sinf(rad);
		mRot.GetElement(2, 1) = sinf(rad);
		mRot.GetElement(2, 2) = cosf(rad);

		return mRot;
	}

	Mat4 Mat4::BuildYRotation(float degrees) {
		Mat4 mRot{};
		float rad = degrees * (PI / 180.f);
		mRot.SetToIdentity();
		mRot.GetElement(0, 0) = cosf(rad);
		mRot.GetElement(0, 2) = sinf(rad);
		mRot.GetElement(2, 0) = -sinf(rad);
		mRot.GetElement(2, 2) = cosf(rad);

		return mRot;
	}

	Mat4 Mat4::BuildScaling(float cx, float cy, float cz, float x, float y, float z) {
		Mat4 mScale{};
		mScale.SetToIdentity();
		mScale.GetElement(0, 0) = x;
		mScale.GetElement(1, 1) = y;
		mScale.GetElement(2, 2) = z;

		mScale.GetElement(0, 3) = cx * (1.0f - x);
		mScale.GetElement(1, 3) = cy * (1.0f - y);
		mScale.GetElement(2, 3) = cz * (1.0f - z);
		return mScale;
	}

	Mat4 Mat4::BuildScaling(float x, float y, float z)
	{
		Mat4 mScale{};
		mScale.SetToIdentity();
		mScale.GetElement(0, 0) = x;
		mScale.GetElement(1, 1) = y;
		mScale.GetElement(2, 2) = z;

		return mScale;
	}

	Mat4 Mat4::BuildSymPerspective(float vfov, float a, float n, float f) {
		float cotFov = 1.f / tanf(vfov / 2.f);
		Mat4 mSym{ cotFov / a, 0.f,	   0.f,					 0.f,
				   0.f,		   cotFov, 0.f,					 0.f,
				   0.f,		   0.f,    -((f + n) / (f - n)), -((2 * n * f) / (f - n)),
				   0.f,		   0.f,    -1.f,				 0.f };

		return mSym;
	}

	Mat4 Mat4::BuildAsymPerspective(float l, float r, float b, float t, float n, float f) {
		Mat4 mAsym{ (2.f * n) / (r - l), 0.f,				  (r + l) / (r - l),    0.f,
					0.f,				 (2.f * n) / (t - b), (t + b) / (r - b),    0.f,
					0.f,				 0.f,				  -((f + n) / (f - n)), -((2 * n * f) / (f - n)),
					0.f,				 0.f,				  -1.f,				    0.f };

		return mAsym;
	}

	Mat4 Mat4::BuildOrtho(float l, float r, float b, float t, float n, float f) {
		Mat4 mOrtho{ 2.f / (r - l), 0.f,           0.f,				 -((r + l) / (r - l)),
					 0.f,		    2.f / (t - b), 0.f,				 -((t + b) / (t - b)),
					 0.f,           0.f,           -(2.f / (f - n)), -((f + n) / (f - n)),
					 0.f,		    0.f,			  0.f,			 1.f };

		return mOrtho;
	}

	Mat4 Mat4::BuildViewport(float x, float y, float w, float h) {
		Mat4 mVP{ w / 2.f, 0.f,     0.f,  (w / 2.f) + x,
				  0.f,     h / 2.f, 0.f,  (h / 2.f) + y,
				  0.f,	   0.f,	    0.5f,  0.5f,
				  0.f,	   0.f,	    0.f,   1.f };

		return mVP;
	}

	Mat4 Mat4::BuildNDCToScreen(int w, int h)
	{
		Mat4 ndcToScreen{};
		ndcToScreen.SetToIdentity();

		// Scale NDC to screen space
		ndcToScreen.GetElement(0, 0) = w / 2.0f;  // Scale x
		ndcToScreen.GetElement(1, 1) = h / 2.0f; // Scale y

		// Translate by (windowWidth / 2, windowHeight / 2) to adjust for the screen coordinates
		ndcToScreen.GetElement(0, 3) = w / 2.0f;  // Translate x
		ndcToScreen.GetElement(1, 3) = h / 2.0f; // Translate y

		return ndcToScreen;
	}

	Mat4 Mat4::BuildScreenToNDC(int w, int h)
	{
		Mat4 screenToNDC{};
		screenToNDC.SetToIdentity();

		// Scale the X and Y to range [-1, 1]
		screenToNDC.GetElement(0, 0) = 2.0f / w;  // Scale x
		screenToNDC.GetElement(1, 1) = 2.0f / h; // Scale y

		// Translate by (-1, -1) to adjust for the bottom-left origin
		screenToNDC.GetElement(0, 3) = -1.0f;  // Translate x by -1
		screenToNDC.GetElement(1, 3) = -1.0f;  // Translate y by -1

		return screenToNDC;
	}

	Vec4 Mat4::GetRow4(unsigned int row) const {
		return Vec4(GetElement(row, 0), GetElement(row, 1), GetElement(row, 2), GetElement(row, 3));
	}

	Vec3 Mat4::GetCol3(unsigned int col) const {
		return Vec3(GetElement(0, col), GetElement(1, col), GetElement(2, col));
	}

	Vec4 Mat4::GetCol4(unsigned int col) const {
		return Vec4(GetElement(0, col), GetElement(1, col), GetElement(2, col), GetElement(3, col));
	}

	float Mat4::Cofactor(unsigned int row, unsigned int col) const {
		// Create a 3x3 submatrix
		Mat3 subMatrix = CreateSubMat3(row, col);
		return subMatrix.Determinant();
	}

	inline Mat3 Mat4::CreateSubMat3(unsigned int row, unsigned int col) const {
		Mat3 result;
		unsigned int subRow = 0, subCol = 0;

		for (unsigned int i = 0; i < 4; ++i) {
			if (i == row) continue;  // Skip the specified row
			subCol = 0;
			for (unsigned int j = 0; j < 4; ++j) {
				if (j == col) continue;  // Skip the specified column
				result.GetElement(subRow, subCol) = GetElement(i, j);
				++subCol;
			}
			++subRow;
		}
		return result;
	}

	Mat4 Mat4::BuildTranslation(const Vec3& vec3) {
		return BuildTranslation(vec3.x, vec3.y, vec3.z);
	}

	Mat4 Mat4::BuildRotation(float degrees, float x, float y, float z) {
		Vec3 n{ x, y, z };
		n.Normalize();

		float rad = degrees * (PI / 180.f);
		float cos_d = cosf(rad);
		float one_minus_cos_d = 1.f - cos_d;
		float sin_d = sinf(rad);

		Mat4 mRot{ cos_d + (one_minus_cos_d * (n.x * n.x)),		   (one_minus_cos_d * (n.x * n.y)) + (sin_d * -n.z), (one_minus_cos_d * (n.x * n.z)) + (sin_d * n.y),  0.f,
				  (one_minus_cos_d * (n.x * n.y)) + (sin_d * n.z),  cos_d + (one_minus_cos_d * (n.y * n.y)),		 (one_minus_cos_d * (n.y * n.z)) + (sin_d * -n.x), 0.f,
				  (one_minus_cos_d * (n.x * n.z)) + (sin_d * -n.y), (one_minus_cos_d * (n.y * n.z)) + (sin_d * n.x),  cos_d + (one_minus_cos_d * (n.z * n.z)),         0.f,
				  0.f,												 0.f,											  0.f,											   1.f };

		return mRot;
	}

	Mat4 Mat4::BuildRotation(float degrees, const Vec3& axis) {
		return BuildRotation(degrees, axis.x, axis.y, axis.z);
	}


	Mat4 Mat4::BuildScaling(const Vec3& pivot, const Vec3& scaleFactors) {
		return BuildScaling(pivot.x, pivot.y, pivot.z, scaleFactors.x, scaleFactors.y, scaleFactors.z);
	}

	Mat4 Mat4::BuildViewMtx(const Vec3& eye, const Vec3& tgt, const Vec3& up) {
		Vec3 w = eye - tgt;
		w.Normalize();
		Vec3 u = up.Cross(w);
		u.Normalize();
		Vec3 v = w.Cross(u);

		Mat4 mView{ u.x, u.y, u.z, -u.Dot(eye),
					v.x, v.y, v.z, -v.Dot(eye),
					w.x, w.y, w.z, -w.Dot(eye),
					  0,   0,   0,   1 };

		return mView;
	}

	void Mat4::SetTranslation(const Vec3& t)
	{
		a[12] = t.x;
		a[13] = t.y;
		a[14] = t.z;
	}

	Vec3 Mat4::GetTranslation() const {
		return Vec3(a[12], a[13], a[14]);
	}

	Vec3 Mat4::GetScale() const {
		float scaleX = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
		float scaleY = std::sqrt(a[4] * a[4] + a[5] * a[5] + a[6] * a[6]);
		float scaleZ = std::sqrt(a[8] * a[8] + a[9] * a[9] + a[10] * a[10]);
		return Vec3(scaleX, scaleY, scaleZ);
	}

	Vec3 Mat4::GetRotation() const {
		// Extract scale first
		Vec3 scale = GetScale();

		// Normalize rotation matrix by removing scale
		Mat4 normalized = *this;
		normalized.a[0] /= scale.x;
		normalized.a[1] /= scale.x;
		normalized.a[2] /= scale.x;
		normalized.a[4] /= scale.y;
		normalized.a[5] /= scale.y;
		normalized.a[6] /= scale.y;
		normalized.a[8] /= scale.z;
		normalized.a[9] /= scale.z;
		normalized.a[10] /= scale.z;

		// Extract angles
		float pitch = std::atan2(-normalized.a[6], normalized.a[10]);
		float yaw = std::asin(normalized.a[2]);
		float roll = std::atan2(-normalized.a[1], normalized.a[0]);

		return Vec3(pitch, yaw, roll);
	}

	Mat4 Mat4::operator*(const Mat4& rhs) const
	{
		Mat4 result{};

		for (unsigned int col = 0; col < 4; ++col) {
			for (unsigned int row = 0; row < 4; ++row) {
				float sum = 0.0f;
				for (unsigned int k = 0; k < 4; ++k) {
					sum += GetElement(row, k) * rhs.GetElement(k, col);
				}
				result.GetElement(row, col) = sum;
			}
		}

		return result;
	}

	Mat4 Mat4::operator*(const Mat4& rhs) {
		Mat4 mat4{};
		for (unsigned int i = 0; i < 4; ++i) {
			for (unsigned int j = 0; j < 4; ++j) {
				for (unsigned int k = 0; k < 4; ++k) {
					mat4.GetElement(i, j) += GetElement(i, k) * rhs.GetElement(k, j);
				}
			}
		}

		return mat4;
	}
}