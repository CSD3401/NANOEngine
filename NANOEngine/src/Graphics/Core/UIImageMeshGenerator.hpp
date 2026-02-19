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
        Math::Vec3 Position;    // 12 bytes — location 0
        Math::Vec2 TexCoord;    //  8 bytes — location 1
        Math::Vec4 Color;       // 16 bytes — location 2 (now per-vertex, no uColor uniform)
        uint32_t texHandleLo;   //  4 bytes — location 3 (low 32 bits of ARB bindless handle)
        uint32_t texHandleHi;   //  4 bytes — location 4 (high 32 bits of ARB bindless handle)
        // Total: 44 bytes (up from 36)
    };

    class UIImageMeshGenerator {
    public:
        static std::vector<UIVertex2> GenerateVertices2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

    private:
        static UIVertex2 CreateVertex2(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

        static std::vector<UIVertex2> GenerateSimple2(
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

        static std::vector<UIVertex2> GenerateSliced2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

        static std::vector<UIVertex2> GenerateTiled2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

        static std::vector<UIVertex2> GenerateFilled2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

        static std::vector<UIVertex2> GenerateHorizontalFill2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

        static std::vector<UIVertex2> GenerateVerticalFill2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );

        static std::vector<UIVertex2> GenerateRadialFill2(
            const NE::ECS::Component::UIImage& image,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );
    };

} // namespace NE::Graphics

#endif // UI_IMAGE_MESH_GENERATOR_HPP
