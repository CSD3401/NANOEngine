#ifndef UI_IMAGE_MESH_GENERATOR_HPP
#define UI_IMAGE_MESH_GENERATOR_HPP

#include <vector>
#include <cmath>
#include "ECS/Components/UIImage.hpp"
#include "../../Math/Vec2.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::Graphics {

    // Vertex structure for UI rendering (DEPRECATED - use UIVertex2)
    struct UIVertex {
        float x, y, z;           // Position
        float u, v;              // UV coordinates
        float r, g, b, a;        // Color
    };

    // New vertex format using engine math types
    // NO NORMALS - UI is always unlit/emissive even in world space (Unity approach)
    // For lit panels, use actual 3D geometry with standard Vertex
    struct UIVertex2 {
        Math::Vec3 Position;  // Use Vec3 instead of raw floats (12 bytes)
        Math::Vec2 TexCoord;  // Use Vec2 instead of raw floats (8 bytes)
        Math::Vec4 Color;     // Use Vec4 for color (16 bytes)
        // Total: 36 bytes vs 64 bytes for standard Vertex
    };

    class UIImageMeshGenerator {
    public:
        // Generate vertices based on image type (DEPRECATED - use GenerateVertices2)
        static std::vector<UIVertex> GenerateVertices(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        // NEW: Generate vertices using UIVertex2 (engine math types)
        static std::vector<UIVertex2> GenerateVertices2(
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
            const Math::Vec4& color
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
            const Math::Vec4& color
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

        // Helper to create a single vertex (DEPRECATED)
        static UIVertex CreateVertex(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color
        );

        // NEW: Helper to create UIVertex2
        static UIVertex2 CreateVertex2(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color
        );

        // NEW: Private generation methods for UIVertex2
        static std::vector<UIVertex2> GenerateSimple2(
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        static std::vector<UIVertex2> GenerateSliced2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        static std::vector<UIVertex2> GenerateTiled2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        static std::vector<UIVertex2> GenerateFilled2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );
    };

} // namespace NE::Graphics

#endif // UI_IMAGE_MESH_GENERATOR_HPP
