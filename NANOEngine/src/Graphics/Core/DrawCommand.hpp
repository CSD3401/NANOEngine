#pragma once
#include <memory>
#include <optional>
#include "../Interfaces/IGeometryBuffer.hpp"
#include "Material.hpp"
#include "../../Math/Mat4.hpp"

namespace NE::Graphics {
    struct DrawCommand {
        std::shared_ptr<IGeometryBuffer> mesh;
        std::shared_ptr<Material> material;
        Math::Mat4 transform;
		std::optional<uint32_t> entity; // The caller entity ID. Only used for object picking.
    };
}