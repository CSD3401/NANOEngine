
#ifndef VEC_4_HPP
#define VEC_4_HPP

#pragma warning(push)
#pragma warning(disable : 4201) // Disable warning C4201 for unnamed struct/union

namespace NANOEngine::Math {
    struct Vec2;
    struct Vec3;

    struct Vec4 {
        union {
            struct {
                float x, y, z, w;
            };
            struct {
                float r, g, b, a;
            };
        };
        Vec4(float a = 0, float b = 0, float c = 0, float d = 0);
        Vec4(const Vec4& rhs) = default;
        ~Vec4() = default;

        operator Vec2() const;
        operator Vec3() const;
        inline float& operator[](unsigned int i);
        inline const float& operator[](unsigned int i) const;

        Vec4 operator+(const Vec4& rhs) const;
        Vec4& operator+=(const Vec4& rhs);

        Vec4 operator-(const Vec4& rhs) const;
        Vec4& operator-=(const Vec4& rhs);

        Vec4 operator*(float scalar) const;
        Vec4& operator*=(float scalar);

        Vec4 operator/(float scalar) const;
        Vec4& operator/=(float scalar);
    };

    using Color = Vec4;
}

#pragma warning(pop)
#endif // !VEC_4_HPP
