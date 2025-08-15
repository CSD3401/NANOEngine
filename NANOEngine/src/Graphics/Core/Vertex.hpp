#pragma once
#include "../../../src/Math/Vec2.hpp"
#include "../../../src/Math/Vec3.hpp"

namespace NE::Graphics {
    using namespace NE::Math;

    struct Vertex {
        Vec3 Position;
        Vec3 Normal;
        Vec2 TexCoord;
    };

}
