#ifndef UI_IMAGE_MESH_GENERATOR_HPP
#define UI_IMAGE_MESH_GENERATOR_HPP

#include <vector>
#include <cmath>
#include "ECS/Components/UIImage.hpp"
#include "../../Math/Vec2.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::Graphics {

    // UI vertex format using engine math types
    // NO NORMALS - UI is always unlit/emissive even in world space (Unity approach)
    // For lit panels, use actual 3D geometry with standard Vertex
    struct UIVertex2 {
        Math::Vec3 Position;  // 12 bytes
        Math::Vec2 TexCoord;  // 8 bytes
        Math::Vec4 Color;     // 16 bytes
        // Total: 36 bytes vs 64 bytes for standard Vertex
    };

    class UIImageMeshGenerator {
    public:
        static std::vector<UIVertex2> GenerateVertices2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

    private:
        static UIVertex2 CreateVertex2(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color
        );

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

        static std::vector<UIVertex2> GenerateHorizontalFill2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        static std::vector<UIVertex2> GenerateVerticalFill2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

        static std::vector<UIVertex2> GenerateRadialFill2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );
    };

} // namespace NE::Graphics

#endif // UI_IMAGE_MESH_GENERATOR_HPP
