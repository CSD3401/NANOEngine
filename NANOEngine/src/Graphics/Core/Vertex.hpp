#pragma once
#include "Math/Vec2.hpp"
#include "Math/Vec3.hpp"

namespace NE::Graphics {
    struct Vertex {
        NE::Math::Vec3 position;
        NE::Math::Vec3 normal;
        NE::Math::Vec3 tangents;
        NE::Math::Vec2 texCoord0;
		NE::Math::Vec2 texCoord1;
    };
}
