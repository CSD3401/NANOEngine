#pragma once

#include "Math/Vec3.hpp"
#include "ECS/Components/Light.hpp"

namespace NE::Graphics {
    struct LightGizmoCommand {
        Math::Vec3 position{ 0.0f, 0.0f, 0.0f };
        Math::Vec3 idRGB{ -1.0f, -1.0f, -1.0f };
        ECS::Component::Light::Type lightType = ECS::Component::Light::Type::Point;
    };
}
