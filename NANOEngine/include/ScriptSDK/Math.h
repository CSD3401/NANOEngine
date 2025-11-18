/**
 * @file Math.h
 * @brief SDK-level Math types interface
 *
 * This header provides access to engine math types used in components.
 * Scripts primarily use NE::Scripting::Vec3 which auto-converts to Math::Vec3.
 *
 * CONDITIONAL COMPILATION:
 * - When building NANOEngine.dll: Includes internal Vec3.hpp/Mat4.hpp
 * - When building ChronoGame: Provides standalone SDK declarations
 * This avoids redefinition while maintaining true SDK independence.
 */

#pragma once

#include <ostream>

// Forward declaration of Scripting::Vec3 for implicit conversion
namespace NE {
    namespace Scripting {
        struct Vec3;
    }
}

#ifdef NANOENGINE_EXPORTS
    // Building NANOEngine.dll - use internal engine headers
    #include "../../src/Math/Vec3.hpp"
    #include "../../src/Math/Mat4.hpp"
#else
    // Building ChronoGame - provide standalone SDK declarations

    // NANOENGINE_API macro for DLL import (game scripts only import)
    #ifndef NANOENGINE_API
        #define NANOENGINE_API __declspec(dllimport)
    #endif

    namespace NE {
    namespace Math {

        // Forward declarations
        struct Vec2;
        struct Vec4;
        struct Mat3;
        struct Mat4;

        constexpr const float PI = 3.14159265358979323846f;

        //=========================================================================
        // VEC3 - 3D Vector
        //=========================================================================

        struct Vec3 {
            float x, y, z;

            NANOENGINE_API Vec3(float x = 0, float y = 0, float z = 0) noexcept;
            Vec3(const Vec3& rhs) = default;
            Vec3(Vec3&&) = default;
            Vec3& operator=(Vec3&&) = default;
            ~Vec3() = default;

            // Implicit conversion from Scripting::Vec3
            NANOENGINE_API Vec3(const NE::Scripting::Vec3& other) noexcept;

            // Implicit conversion to Scripting::Vec3
            NANOENGINE_API operator NE::Scripting::Vec3() const noexcept;

            // Assignment operator for Scripting::Vec3
            NANOENGINE_API Vec3& operator=(const NE::Scripting::Vec3& other) noexcept;

            // Arithmetic Operators
            NANOENGINE_API Vec3 operator+(const Vec3& rhs) const noexcept;
            NANOENGINE_API Vec3& operator+=(const Vec3& rhs) noexcept;

            NANOENGINE_API Vec3 operator-(const Vec3& rhs) const noexcept;
            NANOENGINE_API Vec3& operator-=(const Vec3& rhs) noexcept;

            NANOENGINE_API Vec3 operator-() const noexcept;

            NANOENGINE_API Vec3 operator*(float scalar) const noexcept;
            NANOENGINE_API Vec3& operator*=(float scalar) noexcept;
            NANOENGINE_API Vec3 operator*(const Vec3& rhs) const noexcept;
            NANOENGINE_API Vec3& operator*=(const Vec3& rhs) noexcept;

            NANOENGINE_API Vec3 operator/(float scalar) const;
            NANOENGINE_API Vec3& operator/=(float scalar);
            NANOENGINE_API Vec3 operator/(const Vec3& rhs) const noexcept;
            NANOENGINE_API Vec3& operator/=(const Vec3& rhs) noexcept;

            NANOENGINE_API bool operator==(const Vec3& rhs) const noexcept;
            NANOENGINE_API bool operator!=(const Vec3& rhs) const noexcept;

            NANOENGINE_API Vec3& operator=(const Vec3& rhs) noexcept;

            NANOENGINE_API explicit operator Vec2() const;

            // Vector operations
            NANOENGINE_API float Length() const;
            NANOENGINE_API float LengthSquared() const;

            NANOENGINE_API float Dot(const Vec3& rhs) const;
            NANOENGINE_API Vec3 Cross(const Vec3& rhs) const;

            NANOENGINE_API Vec3 Normalized() const;
            NANOENGINE_API Vec3& Normalize();

            float* Data() { return &x; }
            const float* Data() const { return &x; }

            friend NANOENGINE_API std::ostream& operator<<(std::ostream& os, const Vec3& rhs);
            friend NANOENGINE_API Vec3 operator*(float scalar, const Vec3& rhs);
            friend NANOENGINE_API Vec3 operator/(float scalar, const Vec3& rhs);
        };

        //=========================================================================
        // MAT4 - 4x4 Matrix (Column-major order)
        //=========================================================================

        struct Mat4 {
            float a[16] = { 0 };

            Mat4() = default;

            NANOENGINE_API Mat4(const float& e00, const float& e01, const float& e02, const float& e03,
                 const float& e10, const float& e11, const float& e12, const float& e13,
                 const float& e20, const float& e21, const float& e22, const float& e23,
                 const float& e30, const float& e31, const float& e32, const float& e33);

            NANOENGINE_API Mat4(const float arr[16]);
            NANOENGINE_API Mat4(const Mat4& m);

            NANOENGINE_API Mat4& operator=(const Mat4& m);

            // Matrix operations
            NANOENGINE_API Mat4 operator*(const Mat4& rhs);
            NANOENGINE_API Mat4 operator*(float scalar);
            NANOENGINE_API Mat4& operator*=(const Mat4& rhs);

            // Element access (inline - no DLL call needed)
            float& operator[](const unsigned& i) { return a[i]; }
            const float& operator[](const unsigned& i) const { return a[i]; }

            float& GetElement(unsigned int row, unsigned int col) { return a[col * 4 + row]; }
            const float& GetElement(unsigned int row, unsigned int col) const { return a[col * 4 + row]; }

            // Matrix transformations
            NANOENGINE_API void SetToZero();
            NANOENGINE_API void SetToIdentity();
            NANOENGINE_API void SetTo(const float& e00, const float& e01, const float& e02, const float& e03,
                       const float& e10, const float& e11, const float& e12, const float& e13,
                       const float& e20, const float& e21, const float& e22, const float& e23,
                       const float& e30, const float& e31, const float& e32, const float& e33);

            NANOENGINE_API float Determinant() const;

            NANOENGINE_API Mat4& TransposeInPlace();
            NANOENGINE_API Mat4 Transpose() const;

            NANOENGINE_API bool InverseInPlace();
            NANOENGINE_API Mat4 Inverse() const;

            // Static factory methods for common transformations
            NANOENGINE_API static Mat4 BuildTranslation(float x, float y, float z);
            NANOENGINE_API static Mat4 BuildTranslation(const Vec3& xyz);

            NANOENGINE_API static Mat4 BuildZRotation(float degrees);
            NANOENGINE_API static Mat4 BuildXRotation(float degrees);
            NANOENGINE_API static Mat4 BuildYRotation(float degrees);
            NANOENGINE_API static Mat4 BuildRotation(float degrees, float x, float y, float z);
            NANOENGINE_API static Mat4 BuildRotation(float degrees, const Vec3& axis);

            NANOENGINE_API static Mat4 BuildScaling(float cx, float cy, float cz, float x, float y, float z);
            NANOENGINE_API static Mat4 BuildScaling(float x, float y, float z);
            NANOENGINE_API static Mat4 BuildScaling(const Vec3& pivot, const Vec3& scaleFactors);

            NANOENGINE_API static Mat4 BuildViewMtx(const Vec3& eye, const Vec3& tgt, const Vec3& up);
            NANOENGINE_API static Mat4 BuildSymPerspective(float vfov, float aspect, float near, float far);
            NANOENGINE_API static Mat4 BuildAsymPerspective(float l, float r, float b, float t, float n, float f);
            NANOENGINE_API static Mat4 BuildOrtho(float l, float r, float b, float t, float n, float f);
            NANOENGINE_API static Mat4 BuildViewport(float x, float y, float w, float h);
            NANOENGINE_API static Mat4 BuildNDCToScreen(int w, int h);
            NANOENGINE_API static Mat4 BuildScreenToNDC(int w, int h);

            // Matrix-Vector operations
            NANOENGINE_API Vec4 operator*(const Vec4& rhs);
            NANOENGINE_API Vec3 operator*(const Vec3& rhs);

            NANOENGINE_API Vec3 GetRow3(unsigned int) const;
            NANOENGINE_API Vec4 GetRow4(unsigned int) const;
            NANOENGINE_API Vec3 GetCol3(unsigned int) const;
            NANOENGINE_API Vec4 GetCol4(unsigned int) const;

            NANOENGINE_API float Cofactor(unsigned int row, unsigned int col) const;
            NANOENGINE_API Mat3 CreateSubMat3(unsigned int row, unsigned int col) const;

            NANOENGINE_API Vec3 GetTranslation() const;
            NANOENGINE_API Vec3 GetScale() const;
            NANOENGINE_API Vec3 GetRotation() const;

            const float* Data() const { return a; }

            friend NANOENGINE_API std::ostream& operator<<(std::ostream& os, const Mat4& mat);
        };

    } // namespace Math
    } // namespace NE

#endif // NANOENGINE_EXPORTS
