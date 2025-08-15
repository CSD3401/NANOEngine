#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct Collider {
        enum class ShapeType {
            Box,
            Sphere,
            Capsule,
            None
        };

        // Exposed
        ShapeType shapeType{ ShapeType::None };
        Math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f }; // For box
        float radius{ 0.5f };                      // For sphere/capsule
        float height{ 1.0f };                      // For capsule

        NE_REFLECT_BEGIN(Collider)
            NE_REFLECT_FIELD(shapeType),
            NE_REFLECT_FIELD(halfExtents),
            NE_REFLECT_FIELD(radius),
            NE_REFLECT_FIELD(height)
            NE_REFLECT_END()
    };

}