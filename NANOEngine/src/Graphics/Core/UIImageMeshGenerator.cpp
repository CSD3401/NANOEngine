#include "UIImageMeshGenerator.hpp"
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NE::Graphics {

    std::vector<UIVertex> UIImageMeshGenerator::GenerateVertices(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        using ImageType = NE::ECS::Component::UIImage::ImageType;

        switch (image.imageType) {
        case ImageType::SIMPLE:
            return GenerateSimple(x, y, z, width, height, color);

        case ImageType::SLICED:
            return GenerateSliced(image, x, y, z, width, height, color);

        case ImageType::TILED:
            return GenerateTiled(image, x, y, z, width, height, color);

        case ImageType::FILLED:
            return GenerateFilled(image, x, y, z, width, height, color);

        default:
            return GenerateSimple(x, y, z, width, height, color);
        }
    }

    UIVertex UIImageMeshGenerator::CreateVertex(
        float x, float y, float z,
        float u, float v,
        const Math::Vec4& color
    ) {
        return UIVertex{
            x, y, z,
            u, v,
            color.x, color.y, color.z, color.w
        };
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateSimple(
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        // Standard quad: 2 triangles, 6 vertices
        std::vector<UIVertex> vertices;
        vertices.reserve(6);

        // Triangle 1
        vertices.push_back(CreateVertex(x, y, z, 0.0f, 1.0f, color)); // Bottom-left
        vertices.push_back(CreateVertex(x + width, y, z, 1.0f, 1.0f, color)); // Bottom-right
        vertices.push_back(CreateVertex(x + width, y + height, z, 1.0f, 0.0f, color)); // Top-right

        // Triangle 2
        vertices.push_back(CreateVertex(x, y, z, 0.0f, 1.0f, color)); // Bottom-left
        vertices.push_back(CreateVertex(x + width, y + height, z, 1.0f, 0.0f, color)); // Top-right
        vertices.push_back(CreateVertex(x, y + height, z, 0.0f, 0.0f, color)); // Top-left

        return vertices;
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateSliced(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        // 9-slice: divide into 9 sections
        // Corners stay same size, edges stretch in one direction, center stretches both

        std::vector<UIVertex> vertices;
        vertices.reserve(54); // 9 quads × 6 vertices

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

        // X positions: left edge, left border, right border, right edge
        float x0 = x;
        float x1 = x + left;
        float x2 = x + width - right;
        float x3 = x + width;

        // Y positions: bottom edge, bottom border, top border, top edge
        float y0 = y;
        float y1 = y + bottom;
        float y2 = y + height - top;
        float y3 = y + height;

        // UV positions (assuming texture is already 9-sliced in the atlas)
        // If you need to handle sprite atlases, these would need adjustment
        float u0 = 0.0f;
        float u1 = left / width;
        float u2 = (width - right) / width;
        float u3 = 1.0f;

        float v0 = 1.0f; // Bottom in UV space
        float v1 = 1.0f - (bottom / height);
        float v2 = (top / height);
        float v3 = 0.0f; // Top in UV space

        // Helper lambda to add a quad
        auto AddQuad = [&](float qx0, float qy0, float qx1, float qy1,
            float qu0, float qv0, float qu1, float qv1) {
                // Triangle 1
                vertices.push_back(CreateVertex(qx0, qy0, z, qu0, qv0, color));
                vertices.push_back(CreateVertex(qx1, qy0, z, qu1, qv0, color));
                vertices.push_back(CreateVertex(qx1, qy1, z, qu1, qv1, color));

                // Triangle 2
                vertices.push_back(CreateVertex(qx0, qy0, z, qu0, qv0, color));
                vertices.push_back(CreateVertex(qx1, qy1, z, qu1, qv1, color));
                vertices.push_back(CreateVertex(qx0, qy1, z, qu0, qv1, color));
            };

        // Generate 9 quads:
        // Bottom row
        AddQuad(x0, y0, x1, y1, u0, v0, u1, v1); // Bottom-left corner
        AddQuad(x1, y0, x2, y1, u1, v0, u2, v1); // Bottom edge
        AddQuad(x2, y0, x3, y1, u2, v0, u3, v1); // Bottom-right corner

        // Middle row
        AddQuad(x0, y1, x1, y2, u0, v1, u1, v2); // Left edge
        AddQuad(x1, y1, x2, y2, u1, v1, u2, v2); // Center
        AddQuad(x2, y1, x3, y2, u2, v1, u3, v2); // Right edge

        // Top row
        AddQuad(x0, y2, x1, y3, u0, v2, u1, v3); // Top-left corner
        AddQuad(x1, y2, x2, y3, u1, v2, u2, v3); // Top edge
        AddQuad(x2, y2, x3, y3, u2, v2, u3, v3); // Top-right corner

        return vertices;
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateTiled(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        // Tile the texture to fill the area
        std::vector<UIVertex> vertices;

        float tileWidth = width * image.pixelsPerUnitMultiplier;
        float tileHeight = height * image.pixelsPerUnitMultiplier;

        int tilesX = static_cast<int>(std::ceil(width / tileWidth));
        int tilesY = static_cast<int>(std::ceil(height / tileHeight));

        vertices.reserve(tilesX * tilesY * 6);

        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                float qx0 = x + tx * tileWidth;
                float qy0 = y + ty * tileHeight;
                float qx1 = std::min(qx0 + tileWidth, x + width);
                float qy1 = std::min(qy0 + tileHeight, y + height);

                // Calculate UV based on partial tile
                float qu1 = (qx1 - qx0) / tileWidth;
                float qv1 = (qy1 - qy0) / tileHeight;

                // Triangle 1
                vertices.push_back(CreateVertex(qx0, qy0, z, 0.0f, qv1, color));
                vertices.push_back(CreateVertex(qx1, qy0, z, qu1, qv1, color));
                vertices.push_back(CreateVertex(qx1, qy1, z, qu1, 0.0f, color));

                // Triangle 2
                vertices.push_back(CreateVertex(qx0, qy0, z, 0.0f, qv1, color));
                vertices.push_back(CreateVertex(qx1, qy1, z, qu1, 0.0f, color));
                vertices.push_back(CreateVertex(qx0, qy1, z, 0.0f, 0.0f, color));
            }
        }

        return vertices;
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateFilled(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        using FillMethod = NE::ECS::Component::UIImage::FillMethod;

        switch (image.fillMethod) {
        case FillMethod::HORIZONTAL:
            return GenerateHorizontalFill(image, x, y, z, width, height, color);

        case FillMethod::VERTICAL:
            return GenerateVerticalFill(image, x, y, z, width, height, color);

        case FillMethod::RADIAL_90:
        case FillMethod::RADIAL_180:
        case FillMethod::RADIAL_360:
            return GenerateRadialFill(image, x, y, z, width, height, color);

        default:
            return GenerateSimple(x, y, z, width, height, color);
        }
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateHorizontalFill(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        using FillOrigin = NE::ECS::Component::UIImage::FillOrigin;

        if (image.fillAmount <= 0.0f) {
            return std::vector<UIVertex>(); // Empty
        }

        std::vector<UIVertex> vertices;
        vertices.reserve(6);

        float fillWidth = width * image.fillAmount;
        float startX = x;
        float endX = x + fillWidth;
        float u0 = 0.0f;
        float u1 = image.fillAmount;

        // Fill from right
        if (image.fillOrigin == FillOrigin::RIGHT) {
            startX = x + width - fillWidth;
            endX = x + width;
            u0 = 1.0f - image.fillAmount;
            u1 = 1.0f;
        }

        // Triangle 1
        vertices.push_back(CreateVertex(startX, y, z, u0, 1.0f, color));
        vertices.push_back(CreateVertex(endX, y, z, u1, 1.0f, color));
        vertices.push_back(CreateVertex(endX, y + height, z, u1, 0.0f, color));

        // Triangle 2
        vertices.push_back(CreateVertex(startX, y, z, u0, 1.0f, color));
        vertices.push_back(CreateVertex(endX, y + height, z, u1, 0.0f, color));
        vertices.push_back(CreateVertex(startX, y + height, z, u0, 0.0f, color));

        return vertices;
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateVerticalFill(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        using FillOrigin = NE::ECS::Component::UIImage::FillOrigin;

        if (image.fillAmount <= 0.0f) {
            return std::vector<UIVertex>(); // Empty
        }

        std::vector<UIVertex> vertices;
        vertices.reserve(6);

        float fillHeight = height * image.fillAmount;
        float startY = y;
        float endY = y + fillHeight;
        float v0 = 1.0f;
        float v1 = 1.0f - image.fillAmount;

        // Fill from top
        if (image.fillOrigin == FillOrigin::TOP) {
            startY = y + height - fillHeight;
            endY = y + height;
            v0 = image.fillAmount;
            v1 = 0.0f;
        }

        // Triangle 1
        vertices.push_back(CreateVertex(x, startY, z, 0.0f, v0, color));
        vertices.push_back(CreateVertex(x + width, startY, z, 1.0f, v0, color));
        vertices.push_back(CreateVertex(x + width, endY, z, 1.0f, v1, color));

        // Triangle 2
        vertices.push_back(CreateVertex(x, startY, z, 0.0f, v0, color));
        vertices.push_back(CreateVertex(x + width, endY, z, 1.0f, v1, color));
        vertices.push_back(CreateVertex(x, endY, z, 0.0f, v1, color));

        return vertices;
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateRadialFill(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        using FillMethod = NE::ECS::Component::UIImage::FillMethod;
        using FillOrigin = NE::ECS::Component::UIImage::FillOrigin;

        if (image.fillAmount <= 0.0f) {
            return std::vector<UIVertex>(); // Empty
        }

        // Determine max angle based on fill method
        float maxAngle = 0.0f;
        switch (image.fillMethod) {
        case FillMethod::RADIAL_90:  maxAngle = M_PI * 0.5f; break;  // 90 degrees
        case FillMethod::RADIAL_180: maxAngle = M_PI; break;         // 180 degrees
        case FillMethod::RADIAL_360: maxAngle = M_PI * 2.0f; break;  // 360 degrees
        default: maxAngle = M_PI * 2.0f;
        }

        float fillAngle = maxAngle * image.fillAmount;

        // Determine starting angle based on origin
        float startAngle = 0.0f;
        switch (image.fillOrigin) {
        case FillOrigin::BOTTOM_RADIAL: startAngle = M_PI * 1.5f; break; // -90 degrees (bottom)
        case FillOrigin::RIGHT_RADIAL:  startAngle = 0.0f; break;        // 0 degrees (right)
        case FillOrigin::TOP_RADIAL:    startAngle = M_PI * 0.5f; break; // 90 degrees (top)
        case FillOrigin::LEFT_RADIAL:   startAngle = M_PI; break;        // 180 degrees (left)
        default: startAngle = 0.0f;
        }

        // Adjust for clockwise/counter-clockwise
        float angleDirection = image.fillClockwise ? -1.0f : 1.0f;

        // Center point
        float centerX = x + width * 0.5f;
        float centerY = y + height * 0.5f;

        // Generate radial mesh
        std::vector<UIVertex> vertices;
        const int segments = std::max(3, static_cast<int>(fillAngle / (M_PI * 2.0f) * 64.0f)); // More segments for smoother circle
        vertices.reserve(segments * 3);

        // Center vertex in UV space
        float centerU = 0.5f;
        float centerV = 0.5f;

        for (int i = 0; i < segments; ++i) {
            float angle1 = startAngle + angleDirection * (fillAngle * i / segments);
            float angle2 = startAngle + angleDirection * (fillAngle * (i + 1) / segments);

            // Calculate positions
            float x1 = centerX + std::cos(angle1) * width * 0.5f;
            float y1 = centerY + std::sin(angle1) * height * 0.5f;
            float x2 = centerX + std::cos(angle2) * width * 0.5f;
            float y2 = centerY + std::sin(angle2) * height * 0.5f;

            // Calculate UVs
            float u1 = 0.5f + std::cos(angle1) * 0.5f;
            float v1 = 0.5f + std::sin(angle1) * 0.5f;
            float u2 = 0.5f + std::cos(angle2) * 0.5f;
            float v2 = 0.5f + std::sin(angle2) * 0.5f;

            // Create triangle from center
            vertices.push_back(CreateVertex(centerX, centerY, z, centerU, centerV, color));
            vertices.push_back(CreateVertex(x1, y1, z, u1, v1, color));
            vertices.push_back(CreateVertex(x2, y2, z, u2, v2, color));
        }

        return vertices;
    }

} // namespace NE::Graphics