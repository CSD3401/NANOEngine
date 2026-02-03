#pragma once
#include <memory>
#include <optional>
#include "../Interfaces/IGeometryBuffer.hpp"
#include "Material.hpp"
#include "../../Math/Mat4.hpp"

namespace NE::Graphics {
    struct DrawCommand {
        Math::Mat4 transform;
        Math::Vec3 idRGB = Math::Vec3{ -1.0f, -1.0f, -1.0f }; // for object picking
        Math::Vec3 boundsCenterWS = Math::Vec3{ 0.0f, 0.0f, 0.0f };
        std::shared_ptr<IGeometryBuffer> mesh;
        std::shared_ptr<Material> material;

        float boundsRadiusWs = 0.0f;

        bool castsShadow = false;
        bool receivesShadow = false;
    };
}