#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Core/Reflection.hpp"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace NANOEngine::ECS::Component {

    struct Collider {
        enum class ShapeType {
            Box,
            Sphere,
            Capsule
        };

        // Exposed
        ShapeType shapeType{ ShapeType::Box };
        Math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f }; // For box
        float radius{ 0.5f };                      // For sphere/capsule
        float height{ 1.0f };                      // For capsule

        // Internal
        JPH::RefConst<JPH::Shape> shape{ nullptr };

        NE_REFLECT_BEGIN(Collider)
            NE_REFLECT_FIELD(shapeType),
            NE_REFLECT_FIELD(halfExtents),
            NE_REFLECT_FIELD(radius),
            NE_REFLECT_FIELD(height)
            NE_REFLECT_END()
    };

}