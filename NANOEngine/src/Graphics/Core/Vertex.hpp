#pragma once
#include "../../../src/Math/Vec2.hpp"
#include "../../../src/Math/Vec3.hpp"

namespace NE::Graphics {
    using namespace NE::Math;

    constexpr int MAX_BONE_INFLUENCE = 4;


    struct Vertex {
        Vec3 Position;
        Vec3 Normal;
        Vec2 TexCoord;

        int   BoneIDs[MAX_BONE_INFLUENCE] = { 0, 0, 0, 0 };
        float Weights[MAX_BONE_INFLUENCE] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

}
