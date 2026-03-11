#include "pch.h"
#include "Frustum.hpp"

using NE::Math::Vec3;
using NE::Math::Vec4;
using NE::Math::Mat4;

namespace NE::Graphics {

    Frustum Frustum::ExtractPlanesFromVP(const Mat4& vp) {
        // extract rows from column major VP mat4
        Vec4 r0 = vp.GetRow4(0);
        Vec4 r1 = vp.GetRow4(1);
        Vec4 r2 = vp.GetRow4(2);
        Vec4 r3 = vp.GetRow4(3);

        Frustum f;

        auto setPlane = [&](int idx, const Vec4& v) {
            Plane& p = f.planes[idx];

            p.n = Vec3{ v.x, v.y, v.z };
            p.d = v.w;

            float len = p.n.Length();
            if (std::abs(len) > FLT_EPSILON)
            {
                p.n = p.n.Normalized();
                p.d /= len;
            }
            };

        setPlane(Left, r3 + r0);
        setPlane(Right, r3 - r0);
        setPlane(Bottom, r3 + r1);
        setPlane(Top, r3 - r1);
        setPlane(Near, r3 + r2);
        setPlane(Far, r3 - r2);

        return f;
    }

    bool Frustum::IntersectsSphere(const Vec3& center, float radius) const {
        if (radius <= 0) return true;

        for (int i = 0; i < Count; ++i) {
            const Plane& p = planes[i];
            const float dist = p.n.Dot(center) + p.d;

            if (dist < -radius) return false;
        }

        return true;
    }

    bool Frustum::IntersectsAABB(const Vec3& minWS, const Vec3& maxWS) const {
        // compute all 8 corners of the AABB box
        Vec3 corners[8] = {
            {minWS.x, minWS.y, minWS.z},
            {maxWS.x, minWS.y, minWS.z},
            {minWS.x, maxWS.y, minWS.z},
            {maxWS.x, maxWS.y, minWS.z},
            {minWS.x, minWS.y, maxWS.z},
            {maxWS.x, minWS.y, maxWS.z},
            {minWS.x, maxWS.y, maxWS.z},
            {maxWS.x, maxWS.y, maxWS.z},
        };

        // for each plane, check if all the corners are behind it
        for (int i = 0; i < Count; ++i)
        {
            const Plane& p = planes[i];

            bool isAllOutside = true;

            for (const Vec3& c : corners)
            {
                float dist = p.n.Dot(c) + p.d;
                if (dist >= -FLT_EPSILON) // corner is infront of the plane (inside frustum)
                {
                    isAllOutside = false;
                    break;
                }
            }

            if (isAllOutside)
            {
                return false; // completely outside this plane
            }
        }

        return true; // inside or intersecting
    }

} // namespace NE::Graphics
