/**
 * @file Components.h
 * @brief SDK-level component definitions for scripting
 *
 * This header defines component structures for script access.
 * These definitions are binary-compatible with the engine's internal components.
 *
 * NOTE: These do NOT include reflection macros - reflection is only needed by the engine.
 * Scripts just need field access, which these structures provide.
 */

#pragma once

#include <functional>
#include <cstdint>
#include <limits>

// Include Math types - Components use Vec3 and Mat4
#include "Math.h"
#include "ECS.h"

namespace NE {
namespace ECS {
namespace Component {

    //=========================================================================
    // TRANSFORM COMPONENT
    //=========================================================================

    /// Transform component - position, rotation, and scale of an entity
    /// Binary-compatible with engine's internal Transform component
    struct Transform {
        // Exposed fields
        Math::Vec3 localPosition{ 0.f, 0.f, 0.f };
        Math::Vec3 localScale{ 1.f, 1.f, 1.f };
        Math::Vec3 localRotationEuler{ 0.f, 0.f,0.f };

        // Internal fields
        bool isDirty = true;        ///< Flag indicating transform needs recalculation
        Math::Mat4 modelMatrix{};   ///< Cached model matrix
        Math::Mat4 parent{};        ///< Parent transform matrix
    };

    //=========================================================================
    // LIGHT COMPONENT
    //=========================================================================

    /// Light component - illumination properties
    /// Binary-compatible with engine's internal Light component
    struct Light {
        enum Type {
            Directional,
            Point,
            Spot
        };

        // Internal (Set by transform component)
        Math::Vec3 position{ 0.f, 0.f, 0.f };   ///< Light position (set by transform)

        // Exposed Shared
        Type type = Type::Directional;          ///< Type of light
        Math::Vec3 color{ 1.f, 1.f, 1.f };     ///< RGB color (0-1 range)
        float intensity{ 1.f };                 ///< Light brightness multiplier

        // Directional
        Math::Vec3 direction{ 0.f, -1.f, 0.f }; ///< Light direction

        // Spot Light
        float innerCutoff{ 0.91f };             ///< Inner cutoff angle (cosine value)
        float outerCutoff{ 0.82f };             ///< Outer cutoff angle (cosine value)

        // Attenuation parameters
        float constant{ 1.f };                  ///< Constant attenuation factor
        float linear{ 0.f };                    ///< Linear attenuation factor
        float quadratic{ 1.f };                 ///< Quadratic attenuation factor
    };

    //=========================================================================
    // COLLIDER COMPONENT
    //=========================================================================

    /// Collider component - physics collision shape
    /// Binary-compatible with engine's internal Collider component
    struct Collider {
        enum class ShapeType {
            Box,
            Sphere,
            Capsule,
            Mesh,
            None
        };

        // Exposed properties
        ShapeType shapeType{ ShapeType::Box };      ///< Type of collision shape
        Math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f }; ///< Half extents for box collider
        float radius{ 0.5f };                       ///< Radius for sphere/capsule
        float height{ 1.0f };                       ///< Height for capsule

        // Internal - collision callbacks
        std::function<void(Entity otherEntity)> onCollisionEnter;
        std::function<void(Entity otherEntity)> onCollisionStay;
        std::function<void(Entity otherEntity)> onCollisionExit;

        // Internal - dirty flags for change detection
        bool isShapeDirty = true;               ///< True when shape needs update
        bool isPropertiesDirty = true;          ///< True when properties need update

        // Internal - store previous values for comparison
        ShapeType previousShapeType = ShapeType::Box;
        Math::Vec3 previousHalfExtents{ 0.5f, 0.5f, 0.5f };
        float previousRadius = 0.5f;
        float previousHeight = 1.0f;
    };

} // namespace Component
} // namespace ECS
} // namespace NE
