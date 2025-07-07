#pragma once

#include "../../Math/Vec3.hpp"
#include "../../Core/Reflection.hpp"

namespace NANOEngine::ECS::Component {

    struct PointLight {
        Math::Vec3 position{ 0.f, 0.f, 0.f };
        Math::Vec3 color{ 1.f, 1.f, 1.f };
        float intensity{ 1.f };
        float constant{ 1.f };
        float linear{ 0.f };
        float quadratic{ 1.f };

        NE_REFLECT_BEGIN(PointLight)
            NE_REFLECT_FIELD(position),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD(intensity),
            NE_REFLECT_FIELD(constant),
            NE_REFLECT_FIELD(linear),
            NE_REFLECT_FIELD(quadratic)
            NE_REFLECT_END()
    };

}