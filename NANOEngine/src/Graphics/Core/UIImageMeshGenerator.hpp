#ifndef UI_IMAGE_MESH_GENERATOR_HPP
#define UI_IMAGE_MESH_GENERATOR_HPP

#include <vector>
#include <cmath>
#include "ECS/Components/UIImage.hpp"
#include "../../Math/Vec2.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::Graphics {

    // Vertex structure for UI rendering
    struct UIVertex {
        float x, y, z;           // Position
        float u, v;              // UV coordinates
        float r, g, b, a;        // Color
    };

    class UIImageMeshGenerator {
    public:
        // Generate vertices based on image type
        static std::vector<UIVertex> GenerateVertices(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

    private:
        // Simple quad (no special processing)
        static std::vector<UIVertex> GenerateSimple(
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            bool preserveAspect = false,
            float textureWidth = 0.0f,
            float textureHeight = 0.0f
        );

        // 9-slice scaling
        static std::vector<UIVertex> GenerateSliced(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        // Tiled texture
        static std::vector<UIVertex> GenerateTiled(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        // Filled images
        static std::vector<UIVertex> GenerateFilled(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            float textureWidth = 0.0f,
            float textureHeight = 0.0f
        );

        // Specific fill methods
        static std::vector<UIVertex> GenerateHorizontalFill(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        static std::vector<UIVertex> GenerateVerticalFill(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        static std::vector<UIVertex> GenerateRadialFill(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        // Helper to create a single vertex
        static UIVertex CreateVertex(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color
        );
    };

} // namespace NE::Graphics

#endif // UI_IMAGE_MESH_GENERATOR_HPP
