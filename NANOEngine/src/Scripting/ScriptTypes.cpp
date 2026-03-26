#include "pch.h"
/**
 * @file ScriptTypes.cpp
 * @brief Implementation of ScriptSDK types
 */

#include "../../include/ScriptSDK/ScriptTypes.h"
#include "../Math/Vec3.hpp"
#include "../Math/Vec4.hpp"
#include <cmath>

namespace NE {
namespace Scripting {

    //=========================================================================
    // Vec3 Implementation
    //=========================================================================

    // Conversion from engine Math::Vec3
    Vec3::Vec3(const NE::Math::Vec3& other) : x(other.x), y(other.y), z(other.z) {}

    // Conversion to engine Math::Vec3
    Vec3::operator NE::Math::Vec3() const {
        return NE::Math::Vec3(x, y, z);
    }

    // Assignment operator for Math::Vec3
    Vec3& Vec3::operator=(const NE::Math::Vec3& other) {
        x = other.x;
        y = other.y;
        z = other.z;
        return *this;
    }

    float Vec3::Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    // Note: Normalized() is now inline in ScriptTypes.h

    void Vec3::Normalize() {
        float len = Length();
        if (len < 0.0001f) {
            x = y = z = 0;
            return;
        }
        x /= len;
        y /= len;
        z /= len;
    }

    Vec3 Vec3::Cross(const Vec3& other) const {
        return Vec3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    //=========================================================================
    // Vec4 Implementation
    //=========================================================================

    // Conversion from engine Math::Vec4
    Vec4::Vec4(const NE::Math::Vec4& other) : x(other.x), y(other.y), z(other.z), w(other.w) {}

    // Conversion to engine Math::Vec4
    Vec4::operator NE::Math::Vec4() const {
        return NE::Math::Vec4(x, y, z, w);
    }

    // Assignment operator for Math::Vec4
    Vec4& Vec4::operator=(const NE::Math::Vec4& other) {
        x = other.x;
        y = other.y;
        z = other.z;
        w = other.w;
        return *this;
    }

} // namespace Scripting
} // namespace NE
