/**
 * @file ScriptTypes.cpp
 * @brief Implementation of ScriptSDK types
 */

#include "../../include/ScriptSDK/ScriptTypes.h"
#include <cmath>

namespace NE {
namespace Scripting {

    //=========================================================================
    // Vec3 Implementation
    //=========================================================================

    float Vec3::Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vec3 Vec3::Normalized() const {
        float len = Length();
        if (len < 0.0001f) {
            return Vec3(0, 0, 0);
        }
        return Vec3(x / len, y / len, z / len);
    }

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

} // namespace Scripting
} // namespace NE
