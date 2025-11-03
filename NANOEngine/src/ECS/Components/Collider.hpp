#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Core/Reflection.hpp"
#include <functional>
#include "../../ECS/Core/Entity.hpp"


namespace NE::ECS::Component {

    struct Collider {
        enum class ShapeType {
            Box,
            Sphere,
            Capsule,
            None 
        };

        // Exposed
        ShapeType shapeType{ ShapeType::Box };
        Math::Vec3 halfExtents{ 0.5f, 0.5f, 0.5f }; // For box
        float radius{ 0.5f };                      // For sphere/capsule
        float height{ 1.0f };                      // For capsule

		// INTERNAL NOT FOR REFLECTION
        
        // Collision callbacks (like Unity's MonoBehaviour)
        std::function<void(Entity otherEntity)> onCollisionEnter;
        std::function<void(Entity otherEntity)> onCollisionStay;
        std::function<void(Entity otherEntity)> onCollisionExit;

        // Dirty flags for change detection
        bool isShapeDirty = true;    // True for new colliders
        bool isPropertiesDirty = true;

        // Store previous values for comparison
        ShapeType previousShapeType = ShapeType::Box;
        Math::Vec3 previousHalfExtents{ 0.5f, 0.5f, 0.5f };
        float previousRadius = 0.5f;
        float previousHeight = 1.0f;



        NE_REFLECT_BEGIN(Collider)
            NE_REFLECT_FIELD(shapeType),
            NE_REFLECT_FIELD(halfExtents),
            NE_REFLECT_FIELD(radius),
            NE_REFLECT_FIELD(height)
        NE_REFLECT_END()
    };

}