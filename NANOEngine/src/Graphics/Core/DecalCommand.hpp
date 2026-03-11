#pragma once

#include <memory>
#include "../../Math/Mat4.hpp"
#include "../../Math/Vec3.hpp"

namespace NE::Graphics {
    class Material;

    struct DecalCommand {
        Math::Mat4 model;
        Math::Mat4 invModel;
        std::shared_ptr<Material> material;
        Math::Vec3 positionWS{ 0.0f, 0.0f, 0.0f };
        float drawDistance = 100.0f;
        float startFadeDistance = 80.0f;
    };
}
