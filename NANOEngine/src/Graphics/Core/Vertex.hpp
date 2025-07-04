#pragma once
#include "../../src/Math/Vec2.hpp"
#include "../../src/Math/Vec3.hpp"

namespace NANOEngine::Graphics {
    using namespace NANOEngine::Math;

    struct Vertex {
        Vec3 Position;
        Vec3 Normal;
        Vec2 TexCoord;
    };

}
