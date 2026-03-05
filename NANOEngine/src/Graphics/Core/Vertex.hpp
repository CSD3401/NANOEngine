#pragma once
#include "Math/Vec2.hpp"
#include "Math/Vec3.hpp"

namespace NE::Graphics {
    struct Vertex {
        NE::Math::Vec3 Position;
        NE::Math::Vec3 Normal;
        NE::Math::Vec2 TexCoord;
    };
}
