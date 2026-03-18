#pragma once
#include <memory>
#include <optional>
#include <limits>
#include "../Interfaces/IGeometryBuffer.hpp"
#include "Material.hpp"
#include "Math/Mat4.hpp"
#include "Math/Vec2.hpp"

namespace NE::Graphics {
	struct ParticleInstanceData; // forward declaration

    struct ScissorRect 
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        bool operator==(const ScissorRect& other) const {
            return x == other.x && y == other.y && width == other.width && height == other.height;
        }
        bool operator!=(const ScissorRect& other) const { return !(*this == other); }
    };

    struct DrawCommand 
    {
        Math::Mat4 transform;
		Math::Vec3 idRGB = Math::Vec3{ -1.0f, -1.0f, -1.0f };
		Math::Vec3 boundsCenterWS = Math::Vec3{ 0.0f, 0.0f, 0.0f };
        std::shared_ptr<IGeometryBuffer> mesh;
        std::shared_ptr<Material> material;

		float boundsRadiusWs = 0.0f;

        bool castsShadow = false;
        bool receivesShadow = false;
        bool hasUv1 = false;
        bool lightmapEnabled = false;
        std::uint32_t lightmapPageSlot = std::numeric_limits<std::uint32_t>::max();
        Math::Vec2 lightmapUvScale = Math::Vec2{ 1.0f, 1.0f };
        Math::Vec2 lightmapUvOffset = Math::Vec2{ 0.0f, 0.0f };

        // Optional scissor rect for UI clipping (RectMask2D)
        std::optional<ScissorRect> scissorRect;

        // Enable depth testing for this command (WorldSpace UI)
        bool enableDepthTest = false;
    };

    struct ParticleDrawCommand 
    {
        // Emitter transform 
		NE::Math::Mat4 emitterModel;

        // Bounds for frustum culling (in WORLD space)
        NE::Math::Vec3 boundsCenterWS{ 0,0,0 };
        float boundsRadiusWS = 0.0f;

        // Draw state
        std::shared_ptr<IGeometryBuffer> mesh;
        std::shared_ptr<Material> material;

        // Instance payload
        const ParticleInstanceData* instances = nullptr;
        uint32_t instanceCount = 0;

        // Optional
        bool enableDepthTest = true;
        std::optional<ScissorRect> scissorRect;
    };
}
