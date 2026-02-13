#pragma once

#include "Math/Vec3.hpp"

namespace NE::Graphics {
    struct DecalGizmoCommand {
        Math::Vec3 position{ 0.0f, 0.0f, 0.0f };
        Math::Vec3 idRGB{ -1.0f, -1.0f, -1.0f };
    };
}
