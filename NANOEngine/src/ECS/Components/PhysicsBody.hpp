#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct PhysicsBody {
        uint32_t bodyId = 0;               

        // No reflection needed - internal physics data
    };

}