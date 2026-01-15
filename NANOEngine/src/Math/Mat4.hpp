#ifndef MAT_4_HPP
#define MAT_4_HPP

#include <ostream>
#include "../NANOEngineAPI.hpp"

namespace NE::Math {
	struct Vec3;
	struct Vec4;
	struct Mat3;

	constexpr const float PI = 3.14159265358979323846f;
	constexpr const float DEG_TO_RAD = PI / 180.0f;
	constexpr const float RAD_TO_DEG = 180.0f / PI;

	// Matrix elements stored in memory using column-major order.
	struct NANOENGINE_API Mat4 {
		float a[16] = { 0 };

		Mat4() = default;

		/*!***********************************************************************
		\brief
			Constructs a Mat4 with the initializers.
		\param[in] const float& e00, const float& e01, const float& e02, const float& e03,
					 const float& e10, const float& e11, const float& e12, const float& e13,
					 const float& e20, const float& e21, const float& e22, const float& e23,
					 const float& e30, const float& e31, const float& e32, const float& e33
			Initializers for the Mat4.
		*************************************************************************/
		Mat4(const float& e00, const float& e01, const float& e02, const float& e03,
			const float& e10, const float& e11, const float& e12, const float& e13,
			const float& e20, const float& e21, const float& e22, const float& e23,
			const float& e30, const float& e31, const float& e32, const float& e33);

		Mat4(const float arr[16]);

		/*!***********************************************************************
		\brief
			Copy constructor for Mat4.
		\param[in] const Mat4& m
			Mat4 to copy from.
		*************************************************************************/
		Mat4(const Mat4& m);

		/*!***********************************************************************
		\brief
			Assignment operator for Mat4.
		\param[in] const Mat4& m
			Mat4 to copy from.
		\return
			Reference to Mat4 object with copied values.
		*************************************************************************/
		Mat4& operator=(const Mat4& m);

		/*!***********************************************************************
		\brief
			Matrix-matrix multiplication.
		\param[in] const Mat4& rhs
			Rhs of the matrix multiplication.
		\return
			*this * rhs.
		*************************************************************************/
		Mat4 operator*(const Mat4& rhs) const;
		Mat4 operator*(const Mat4& rhs);

		/*!***********************************************************************
		\brief
			Scalar Matrix multiplication.
		\param[in] float scalar
			Scalar of the matrix multiplication.
		\return
			*this * scalar
		*************************************************************************/
		Mat4 operator*(float scalar);

		/*!***********************************************************************
		\brief
			Matrix-matrix compound multiplication.
		\param[in] const Mat4& rhs
			Rhs of the matrix multiplication.
		\return
			*this = *this * rhs
		*************************************************************************/
		Mat4& operator*=(const Mat4& rhs);

		/*!***********************************************************************
		\brief
			Getter/setter function for element of Matrix at position i.
		\param[in] const unsigned& i
			Position of element to get/set.
		\return
			Reference to the element at position i.
		*************************************************************************/
		float& operator[](const unsigned& i) { return a[i]; }

		/*!***********************************************************************
		\brief
			Getter function for element of const Matrix at position i.
		\param[in] const unsigned& i
			Position of element to get.
		\return
			Const reference to the element at position i.
		*************************************************************************/
		const float& operator[](const unsigned& i) const { return a[i]; }

		/*!***********************************************************************
		\brief
			Set all Matrix values to 0.
		*************************************************************************/
		void SetToZero();

		/*!***********************************************************************
		\brief
			Set Matrix to an identity matrix.
		*************************************************************************/
		void SetToIdentity();

		/*!***********************************************************************
		\brief
			Set Matrix to the specified values in row-major order.
		\param[in] const float& e00, const float& e01, const float& e02, const float& e03,
				   const float& e10, const float& e11, const float& e12, const float& e13,
				   const float& e20, const float& e21, const float& e22, const float& e23,
				   const float& e30, const float& e31, const float& e32, const float& e33
			Values to set Matrix to.
		*************************************************************************/
		void SetTo(const float& e00, const float& e01, const float& e02, const float& e03,
			const float& e10, const float& e11, const float& e12, const float& e13,
			const float& e20, const float& e21, const float& e22, const float& e23,
			const float& e30, const float& e31, const float& e32, const float& e33);

		/*!***********************************************************************
		\brief
			Getter/setter function for element of Matrix at specified row and col.
		\param[in] unsigned int row
			Row of matrix to access.
		\param[in] unsigned int col
			Column of matrix to access.
		\return
			Reference to the element at specified row and column.
		*************************************************************************/
		float& GetElement(unsigned int row, unsigned int col) { return a[col * 4 + row]; }

		/*!***********************************************************************
		\brief
			Getter/setter function for element of const Matrix at specified row
			and col.
		\param[in] unsigned int row
			Row of matrix to access.
		\param[in] unsigned int col
			Column of matrix to access.
		\return
			Const reference to the element at specified row and column.
		*************************************************************************/
		const float& GetElement(unsigned int row, unsigned int col) const { return a[col * 4 + row]; }

		/*!***********************************************************************
		\brief
			Return determinant of Matrix.
		\return
			Determinant of Matrix.
		*************************************************************************/
		float Determinant() const;

		/*!***********************************************************************
		\brief
			Transpose the Matrix in place.
		\return
			Transpose of the current Matrix.
		*************************************************************************/
		Mat4& TransposeInPlace();

		/*!***********************************************************************
		\brief
			Return transpose of Matrix.
		\return
			Transpose of Matrix.
		*************************************************************************/
		Mat4 Transpose() const;

		/*!***********************************************************************
		\brief
			Inverse the Matrix in place.
		\return
			True if inverse is found and false otherwise.
		*************************************************************************/
		bool InverseInPlace();

		/*!***********************************************************************
		\brief
			Return inverse of Matrix.
		\return
			Inverse of Matrix.
		*************************************************************************/
		Mat4 Inverse() const;

		/*!***********************************************************************
		\brief
			Return a translation matrix based on Vec3(x, y, z).
		\param[in] float x, float y, float z
			Vector to translate.
		\return
			Translation matrix based on Vec3(x, y, z).
		*************************************************************************/
		static Mat4	BuildTranslation(float x, float y, float z);

		/*!***********************************************************************
		\brief
			Return a translation matrix based on xyz.
		\param[in] const Vec3& xyz
			Vector to translate.
		\return
			Translation matrix based on xyz.
		*************************************************************************/
		static Mat4	BuildTranslation(const Vec3& xyz);

		/*!***********************************************************************
		\brief
			Return a rotation matrix based on rotation along Z-axis by specified
			degrees.
		\param[in] float degrees
			Degrees to rotate.
		\return
			Rotation matrix based on rotation along Z-axis by specified degrees.
		*************************************************************************/
		static Mat4 BuildZRotation(float degrees);

		/*!***********************************************************************
		\brief
			Return a rotation matrix based on rotation along X-axis by specified
			degrees.
		\param[in] float degrees
			Degrees to rotate.
		\return
			Rotation matrix based on rotation along X-axis by specified degrees.
		*************************************************************************/
		static Mat4 BuildXRotation(float degrees);

		/*!***********************************************************************
		\brief
			Return a rotation matrix based on rotation along Y-axis by specified
			degrees.
		\param[in] float degrees
			Degrees to rotate.
		\return
			Rotation matrix based on rotation along Y-axis by specified degrees.
		*************************************************************************/
		static Mat4 BuildYRotation(float degrees);

		/*!***********************************************************************
		\brief
			Return a rotation matrix based on rotation along the specified axis
			by specified degrees.
		\param[in] float degrees
			Degrees to rotate.
		\param[in] float x, float y, float z
			Axis to rotate.
		\return
			Rotation matrix based on rotation along the specified axis by specified
			degrees.
		*************************************************************************/
		static Mat4	BuildRotation(float degrees, float x, float y, float z);

		/*!***********************************************************************
		\brief
			Return a rotation matrix based on rotation along the specified axis
			by specified degrees.
		\param[in] float degrees
			Degrees to rotate.
		\param[in] const Vec3& axis
			Axis to rotate.
		\return
			Rotation matrix based on rotation along the specified axis by specified
			degrees.
		*************************************************************************/
		static Mat4	BuildRotation(float degrees, const Vec3& axis);

		/*!***********************************************************************
		\brief
			Return a scale matrix based on scaling about the specified pivot point
			and specified scalar vector.
		\param[in] float cx, float cy, float cz
			Pivot point of scale.
		\param[in] float x, float y, float z
			Scale vector.
		\return
			Scale matrix based on scaling about the specified pivot point and
			specified scalar vector.
		*************************************************************************/
		static Mat4	BuildScaling(float cx, float cy, float cz, float x, float y, float z);

		/*!***********************************************************************
		\brief
			Return a scale matrix based on scaling about the specified pivot point
			and specified scalar vector.
		\param[in] float cx, float cy, float cz
			Pivot point of scale.
		\param[in] float x, float y, float z
			Scale vector.
		\return
			Scale matrix based on scaling about the specified pivot point and
			specified scalar vector.
	*************************************************************************/
		static Mat4	BuildScaling(float x, float y, float z);

		/*!***********************************************************************
		\brief
			Return a scale matrix based on scaling about the specified pivot point
			and specified scalar vector.
		\param[in] const Vec3& pivot
			Pivot point of scale.
		\param[in] const Vec3& scaleFactors
			Scale vector.
		\return
			Scale matrix based on scaling about the specified pivot point and
			specified scalar vector.
		*************************************************************************/
		static Mat4	BuildScaling(const Vec3& pivot, const Vec3& scaleFactors);

		/*!***********************************************************************
		\brief
			Return a view (camera) matrix based on the eye, target and up vector.
		\param[in] const Vec3& eye
			Eye (position) of camera.
		\param[in] const Vec3& tgt
			Target vector of camera.
		\param[in] const Vec3& up
			Up vector of camera.
		\return
			View (camera) matrix based on the eye, target and up vector.
		*************************************************************************/
		static Mat4	BuildViewMtx(const Vec3& eye, const Vec3& tgt, const Vec3& up);

		/*!***********************************************************************
		\brief
			Return a symmetric perspective projection matrix based on vertical FOV,
			aspect ratio, near and far plane.
		\param[in] float vfov
			Vertical FOV in radians.
		\param[in] float aspect
			Aspect ratio of viewfinder.
		\param[in] float near, float far
			Near and far plane of the frustum.
		\return
			Symmetric perspective projection matrix based on vertical FOV, aspect
			ratio, near and far plane.
		*************************************************************************/
		static Mat4	BuildSymPerspective(float vfov, float aspect, float near, float far);

		/*!***********************************************************************
		\brief
			Return an asymmetric perspective projection matrix based on the left,
			right, bottom, top, near and far planes.
		\param[in] float l, float r, float b, float t, float n, float f
			Planes of the frustum.
		\return
			Asymmetric perspective projection matrix based on the left, right,
			bottom, top, near and far planes.
		*************************************************************************/
		static Mat4	BuildAsymPerspective(float l, float r, float b, float t, float n, float f);

		/*!***********************************************************************
		\brief
			Return an orthographic projection matrix based on the left, right,
			bottom, top, near and far planes.
		\param[in] float l, float r, float b, float t, float n, float f
			Planes of the frustum.
		\return
			Orthographic projection matrix based on the left, right, bottom, top,
			near and far planes.
		*************************************************************************/
		static Mat4	BuildOrtho(float l, float r, float b, float t, float n, float f);

		/*!***********************************************************************
		\brief
			Return a viewport transformation matrix to convert NDC coordinates to
			viewport coordinates.
		\param[in] float x, float y
			NDC coordinates.
		\param[in] float w, float h
			Width and height of viewport.
		\return
			Viewport transformation matrix to convert NDC coordinates to viewport
			coordinates.
		*************************************************************************/
		static Mat4	BuildViewport(float x, float y, float w, float h);

		static Mat4 BuildNDCToScreen(int w, int h);
		static Mat4 BuildScreenToNDC(int w, int h);

		// Defined in Math.hpp. Include Math.hpp to use these functions.
		/*!***********************************************************************
		\brief
			Matrix-Vec4 multiplication.
		\param[in] const Vec4& rhs
			Vec4 to multiply with.
		\return
			*this * rhs.
		*************************************************************************/
		Vec4 operator*(const Vec4& rhs);
		Vec3 operator*(const Vec3& rhs);

		/*!***********************************************************************
		\brief
			Get an entire row of the Matrix as a Vec3.
		\param[in] unsigned int row
			Row of matrix to get.
		\return
			Entire row of the Matrix as a Vec3.
		*************************************************************************/
		Vec3 GetRow3(unsigned int) const;

		/*!***********************************************************************
		\brief
			Get an entire row of the Matrix as a Vec4.
		\param[in] unsigned int row
			Row of matrix to get.
		\return
			Entire row of the Matrix as a Vec4.
		*************************************************************************/
		Vec4 GetRow4(unsigned int) const;

		/*!***********************************************************************
		\brief
			Get an entire column of the Matrix as a Vec3.
		\param[in] unsigned int col
			Column of matrix to get.
		\return
			Entire column of the Matrix as a Vec3.
		*************************************************************************/
		Vec3 GetCol3(unsigned int) const;

		/*!***********************************************************************
		\brief
			Get an entire column of the Matrix as a Vec4.
		\param[in] unsigned int col
			Column of matrix to get.
		\return
			Entire column of the Matrix as a Vec4.
		*************************************************************************/
		Vec4 GetCol4(unsigned int) const;

		/*!***********************************************************************
		\brief
			Return the cofactor of the Matrix based on the specified row and col.
		\param[in] unsigned int row, unsigned int col
			Row and column of matrix to get cofactor of.
		\return
			Cofactor of the Matrix based on the specified row and col.
		*************************************************************************/
		float Cofactor(unsigned int row, unsigned int col) const;

		/*!***********************************************************************
		\brief
			Return a Mat3 based on this Mat4 by ignoring the specified row and col.
		\param[in] unsigned int row, unsigned int col
			Row and column of matrix to ignore.
		\return
			Mat3 based on this Mat4 by ignoring the specified row and col.
		*************************************************************************/
		Mat3 CreateSubMat3(unsigned int row, unsigned int col) const;

		void SetTranslation(const Vec3& t);
		Vec3 GetTranslation() const;

		Vec3 GetScale() const;

		Vec3 GetRotation() const;

		const float* Data() const { return a; }
		float* Data() { return a; }

		friend std::ostream& operator<<(std::ostream& os, const Mat4& mat) {
			os << "Mat4:\n";
			for (int row = 0; row < 4; ++row) {
				os << "| ";
				for (int col = 0; col < 4; ++col) {
					os << mat.GetElement(row, col) << " ";
				}
				os << "|\n";
			}
			return os;
		}
	};


}

#endif // !MAT_4_HPP