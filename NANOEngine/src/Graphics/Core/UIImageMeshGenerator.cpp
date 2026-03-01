#include "pch.h"
#include "UIImageMeshGenerator.hpp"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NE::Graphics {

    UIVertex2 UIImageMeshGenerator::CreateVertex2(
        float x, float y, float z,
        float u, float v,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    ) {
        uint32_t handleLo = static_cast<uint32_t>(bindlessHandle & 0xFFFFFFFF);
        uint32_t handleHi = static_cast<uint32_t>(bindlessHandle >> 32);
        return UIVertex2{
            Math::Vec3(x, y, z),
            Math::Vec2(u, v),
            color,
            handleLo,
            handleHi
        };
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateVertices2(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        using ImageType = NE::ECS::Component::UIImage::ImageType;

        switch (image.imageType) {
        case ImageType::SIMPLE:
            return GenerateSimple2(x, y, z, width, height, color, bindlessHandle);

        case ImageType::SLICED:
            return GenerateSliced2(image, x, y, z, width, height, color, bindlessHandle);

        case ImageType::TILED:
            return GenerateTiled2(image, x, y, z, width, height, color, bindlessHandle);

        case ImageType::FILLED:
            return GenerateFilled2(image, x, y, z, width, height, color, bindlessHandle);

        default:
            return GenerateSimple2(x, y, z, width, height, color, bindlessHandle);
        }
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateSimple2(
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    ) {
        // Standard quad: 4 vertices (indices will be generated later)
        std::vector<UIVertex2> vertices;
        vertices.reserve(4);

        // Create 4 vertices for a quad (CW winding order)
        // V=0 at top (y), V=1 at bottom (y+height) — matches stbi row order (top-to-bottom)
        vertices.push_back(CreateVertex2(x, y, z, 0.0f, 0.0f, color, bindlessHandle)); // Top-left
        vertices.push_back(CreateVertex2(x + width, y, z, 1.0f, 0.0f, color, bindlessHandle)); // Top-right
        vertices.push_back(CreateVertex2(x + width, y + height, z, 1.0f, 1.0f, color, bindlessHandle)); // Bottom-right
        vertices.push_back(CreateVertex2(x, y + height, z, 0.0f, 1.0f, color, bindlessHandle)); // Bottom-left

        return vertices;
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateSliced2(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        // 9-slice: divide into 9 sections
        // Corners stay same size, edges stretch in one direction, center stretches both
        std::vector<UIVertex2> vertices;
        vertices.reserve(36); // 9 quads x 4 vertices

        float left = image.borderLeft;
        float right = image.borderRight;
        float top = image.borderTop;
        float bottom = image.borderBottom;

        // Prevent borders from being larger than the image
        float totalHorizontal = left + right;
        float totalVertical = top + bottom;

        if (totalHorizontal > width) {
            float scale = width / totalHorizontal;
            left *= scale;
            right *= scale;
        }

        if (totalVertical > height) {
            float scale = height / totalVertical;
            top *= scale;
            bottom *= scale;
        }

        // X positions
        float x0 = x;
        float x1 = x + left;
        float x2 = x + width - right;
        float x3 = x + width;

        // Y positions (Y increases downward in screen space)
        float y0 = y;
        float y1 = y + top;             // top border inset from the top edge
        float y2 = y + height - bottom; // bottom border inset from the bottom edge
        float y3 = y + height;

        // UV positions — V=0 at top (y0), V=1 at bottom (y3)
        float u0 = 0.0f;
        float u1 = left / width;
        float u2 = (width - right) / width;
        float u3 = 1.0f;

        float v0 = 0.0f;
        float v1 = top / height;
        float v2 = 1.0f - (bottom / height);
        float v3 = 1.0f;

        // Helper lambda: emit 4 vertices per quad (indexed by CreateDynamicUIGeometry)
        auto AddQuad = [&](float qx0, float qy0, float qx1, float qy1,
            float qu0, float qv0, float qu1, float qv1) {
                vertices.push_back(CreateVertex2(qx0, qy0, z, qu0, qv0, color, bindlessHandle));
                vertices.push_back(CreateVertex2(qx1, qy0, z, qu1, qv0, color, bindlessHandle));
                vertices.push_back(CreateVertex2(qx1, qy1, z, qu1, qv1, color, bindlessHandle));
                vertices.push_back(CreateVertex2(qx0, qy1, z, qu0, qv1, color, bindlessHandle));
            };

        // Generate 9 quads:
        // Bottom row
        AddQuad(x0, y0, x1, y1, u0, v0, u1, v1);
        AddQuad(x1, y0, x2, y1, u1, v0, u2, v1);
        AddQuad(x2, y0, x3, y1, u2, v0, u3, v1);

        // Middle row
        AddQuad(x0, y1, x1, y2, u0, v1, u1, v2);
        AddQuad(x1, y1, x2, y2, u1, v1, u2, v2);
        AddQuad(x2, y1, x3, y2, u2, v1, u3, v2);

        // Top row
        AddQuad(x0, y2, x1, y3, u0, v2, u1, v3);
        AddQuad(x1, y2, x2, y3, u1, v2, u2, v3);
        AddQuad(x2, y2, x3, y3, u2, v2, u3, v3);

        return vertices;
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateTiled2(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        // Tile the texture to fill the area
        std::vector<UIVertex2> vertices;

        float tileWidth = width * image.pixelsPerUnitMultiplier;
        float tileHeight = height * image.pixelsPerUnitMultiplier;

        int tilesX = static_cast<int>(std::ceil(width / tileWidth));
        int tilesY = static_cast<int>(std::ceil(height / tileHeight));

        vertices.reserve(tilesX * tilesY * 4);

        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                float qx0 = x + tx * tileWidth;
                float qy0 = y + ty * tileHeight;
                float qx1 = std::min(qx0 + tileWidth, x + width);
                float qy1 = std::min(qy0 + tileHeight, y + height);

                float qu1 = (qx1 - qx0) / tileWidth;
                float qv1 = (qy1 - qy0) / tileHeight;

                // V=0 at top (qy0), V=qv1 at bottom (qy1)
                vertices.push_back(CreateVertex2(qx0, qy0, z, 0.0f, 0.0f, color, bindlessHandle));
                vertices.push_back(CreateVertex2(qx1, qy0, z, qu1, 0.0f, color, bindlessHandle));
                vertices.push_back(CreateVertex2(qx1, qy1, z, qu1, qv1, color, bindlessHandle));
                vertices.push_back(CreateVertex2(qx0, qy1, z, 0.0f, qv1, color, bindlessHandle));
            }
        }

        return vertices;
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateFilled2(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        using FillMethod = NE::ECS::Component::UIImage::FillMethod;

        switch (image.fillMethod) {
        case FillMethod::HORIZONTAL:
            return GenerateHorizontalFill2(image, x, y, z, width, height, color, bindlessHandle);

        case FillMethod::VERTICAL:
            return GenerateVerticalFill2(image, x, y, z, width, height, color, bindlessHandle);

        case FillMethod::RADIAL_90:
        case FillMethod::RADIAL_180:
        case FillMethod::RADIAL_360:
            return GenerateRadialFill2(image, x, y, z, width, height, color, bindlessHandle);

        default:
            return GenerateSimple2(x, y, z, width, height, color, bindlessHandle);
        }
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateHorizontalFill2(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        using FillOrigin = NE::ECS::Component::UIImage::FillOrigin;

        if (image.fillAmount <= 0.0f) {
            return std::vector<UIVertex2>();
        }

        std::vector<UIVertex2> vertices;
        vertices.reserve(4);

        float fillWidth = width * image.fillAmount;
        float startX = x;
        float endX = x + fillWidth;
        float u0 = 0.0f;
        float u1 = image.fillAmount;

        if (image.fillOrigin == FillOrigin::RIGHT) {
            startX = x + width - fillWidth;
            endX = x + width;
            u0 = 1.0f - image.fillAmount;
            u1 = 1.0f;
        }

        // V=0 at top (y), V=1 at bottom (y+height)
        vertices.push_back(CreateVertex2(startX, y, z, u0, 0.0f, color, bindlessHandle));
        vertices.push_back(CreateVertex2(endX, y, z, u1, 0.0f, color, bindlessHandle));
        vertices.push_back(CreateVertex2(endX, y + height, z, u1, 1.0f, color, bindlessHandle));
        vertices.push_back(CreateVertex2(startX, y + height, z, u0, 1.0f, color, bindlessHandle));

        return vertices;
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateVerticalFill2(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        using FillOrigin = NE::ECS::Component::UIImage::FillOrigin;

        if (image.fillAmount <= 0.0f) {
            return std::vector<UIVertex2>();
        }

        std::vector<UIVertex2> vertices;
        vertices.reserve(4);

        float fillHeight = height * image.fillAmount;
        // Default: FillOrigin::BOTTOM — visible region grows upward from bottom
        // In Y-down screen space, bottom = large y, so region is at [y+height-fill, y+height]
        float startY = y + height - fillHeight;
        float endY = y + height;
        float v0 = 1.0f - image.fillAmount;  // V at top of visible region
        float v1 = 1.0f;                     // V at bottom of element

        if (image.fillOrigin == FillOrigin::TOP) {
            // FillOrigin::TOP — visible region grows downward from top
            startY = y;
            endY = y + fillHeight;
            v0 = 0.0f;               // V=0 at top of element
            v1 = image.fillAmount;   // V at bottom of visible region
        }

        vertices.push_back(CreateVertex2(x, startY, z, 0.0f, v0, color, bindlessHandle));
        vertices.push_back(CreateVertex2(x + width, startY, z, 1.0f, v0, color, bindlessHandle));
        vertices.push_back(CreateVertex2(x + width, endY, z, 1.0f, v1, color, bindlessHandle));
        vertices.push_back(CreateVertex2(x, endY, z, 0.0f, v1, color, bindlessHandle));

        return vertices;
    }

    std::vector<UIVertex2> UIImageMeshGenerator::GenerateRadialFill2(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        using FillMethod = NE::ECS::Component::UIImage::FillMethod;
        using FillOrigin = NE::ECS::Component::UIImage::FillOrigin;

        if (image.fillAmount <= 0.0f) {
            return std::vector<UIVertex2>();
        }

        float maxAngle = 0.0f;
        switch (image.fillMethod) {
        case FillMethod::RADIAL_90:  maxAngle = static_cast<float>(M_PI * 0.5); break;
        case FillMethod::RADIAL_180: maxAngle = static_cast<float>(M_PI); break;
        case FillMethod::RADIAL_360: maxAngle = static_cast<float>(M_PI * 2.0); break;
        default: maxAngle = static_cast<float>(M_PI * 2.0);
        }

        float fillAngle = maxAngle * image.fillAmount;

        float startAngle = 0.0f;
        switch (image.fillOrigin) {
        case FillOrigin::BOTTOM_RADIAL: startAngle = static_cast<float>(M_PI * 1.5); break;
        case FillOrigin::RIGHT_RADIAL:  startAngle = 0.0f; break;
        case FillOrigin::TOP_RADIAL:    startAngle = static_cast<float>(M_PI * 0.5); break;
        case FillOrigin::LEFT_RADIAL:   startAngle = static_cast<float>(M_PI); break;
        default: startAngle = 0.0f;
        }

        float angleDirection = image.fillClockwise ? -1.0f : 1.0f;

        float centerX = x + width * 0.5f;
        float centerY = y + height * 0.5f;

        const int segments = std::max(3, static_cast<int>(fillAngle / static_cast<float>(M_PI * 2.0) * 64.0f));

        // Each segment becomes a degenerate quad: center, arc1, arc2, center
        // Index pattern (0,1,2, 2,3,0) draws one real triangle and one degenerate
        std::vector<UIVertex2> vertices;
        vertices.reserve(segments * 4);

        float centerU = 0.5f;
        float centerV = 0.5f;

        for (int i = 0; i < segments; ++i) {
            float angle1 = startAngle + angleDirection * (fillAngle * i / segments);
            float angle2 = startAngle + angleDirection * (fillAngle * (i + 1) / segments);

            float px1 = centerX + std::cos(angle1) * width * 0.5f;
            float py1 = centerY + std::sin(angle1) * height * 0.5f;
            float px2 = centerX + std::cos(angle2) * width * 0.5f;
            float py2 = centerY + std::sin(angle2) * height * 0.5f;

            float u1 = 0.5f + std::cos(angle1) * 0.5f;
            float pv1 = 0.5f + std::sin(angle1) * 0.5f;
            float u2 = 0.5f + std::cos(angle2) * 0.5f;
            float pv2 = 0.5f + std::sin(angle2) * 0.5f;

            vertices.push_back(CreateVertex2(centerX, centerY, z, centerU, centerV, color, bindlessHandle));
            vertices.push_back(CreateVertex2(px1, py1, z, u1, pv1, color, bindlessHandle));
            vertices.push_back(CreateVertex2(px2, py2, z, u2, pv2, color, bindlessHandle));
            vertices.push_back(CreateVertex2(centerX, centerY, z, centerU, centerV, color, bindlessHandle)); // degenerate 4th vertex
        }

        return vertices;
    }

} // namespace NE::Graphics