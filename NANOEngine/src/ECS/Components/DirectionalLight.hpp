#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Core/Reflection.hpp"

namespace NANOEngine::ECS::Component {

    struct DirectionalLight {
        Math::Vec3 direction{ 0.f, -1.f, 0.f };
        Math::Vec3 color{ 1.f, 1.f, 1.f };
        float intensity{ 1.f };

        NE_REFLECT_BEGIN(DirectionalLight)
            NE_REFLECT_FIELD(direction),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD(intensity)
            NE_REFLECT_END()
    };

}