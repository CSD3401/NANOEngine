#pragma once
#include "../../Math/Vec2.hpp"
#include "../../Math/Vec3.hpp"

namespace NANOEngine::Graphics {
    using namespace NANOEngine::Math;

    struct Vertex {
        Vec3 Position;
        Vec3 Normal;
        Vec2 TexCoord;
    };

}
