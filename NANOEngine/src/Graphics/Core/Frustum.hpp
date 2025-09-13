#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP

#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"
#include "../../Math/Mat4.hpp"

namespace NE::Graphics {

    struct Plane {
        NE::Math::Vec3 n; // n must be normalized
        float d; // plane equation: Ax + By + Cz + d = dot(n, P) + d = 0
    };

    struct Frustum {
        enum FrustumPlane {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far,
            Count
        };

        Plane planes[Count];

        // extract planes from column-major VP = P * V
        static Frustum ExtractPlanesFromVP(const NE::Math::Mat4& vp);

        // test bounding sphere interesection with frustum in world space
        bool IntersectsSphere(const NE::Math::Vec3& center, float radius) const;

        // test axis-aligned bounding box intersection with frustum in world space
        bool IntersectsAABB(const NE::Math::Vec3& minWS, const NE::Math::Vec3& maxWS) const;
    };

} // namespace NE::Graphics
#endif // END FRUSTUM_HPP
