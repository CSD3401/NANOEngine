#pragma once
#include <memory>
#include <optional>
#include "../Interfaces/IGeometryBuffer.hpp"
#include "Material.hpp"
#include "../../Math/Mat4.hpp"

namespace NE::Graphics {

    struct ScissorRect {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        bool operator==(const ScissorRect& other) const {
            return x == other.x && y == other.y && width == other.width && height == other.height;
        }
        bool operator!=(const ScissorRect& other) const { return !(*this == other); }
    };

    struct DrawCommand {
        Math::Mat4 transform;
		Math::Vec3 idRGB = Math::Vec3{ -1.0f, -1.0f, -1.0f };
		Math::Vec3 boundsCenterWS = Math::Vec3{ 0.0f, 0.0f, 0.0f };
        std::shared_ptr<IGeometryBuffer> mesh;
        std::shared_ptr<Material> material;

		float boundsRadiusWs = 0.0f;

        bool castsShadow = false;
        bool receivesShadow = false;

        // Optional scissor rect for UI clipping (RectMask2D)
        std::optional<ScissorRect> scissorRect;
    };
}