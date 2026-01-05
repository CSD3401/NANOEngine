#include "UIImageMeshGenerator.hpp"
#include <algorithm>
#include "../../ResourceManagement/ResourceManager.hpp"
#include "../OpenGL/GLTexture.hpp"
#include <iostream>

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
        
        // Get texture dimensions (use cached if available, otherwise try to load)
        float textureWidth = image.cachedTextureWidth;
        float textureHeight = image.cachedTextureHeight;
        
        // Fallback: if dimensions are missing but we have a texture UUID, try to load them
        if ((textureWidth <= 0.0f || textureHeight <= 0.0f) && !image.textureUUID.empty()) {
            auto texture = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::OpenGL::GLTexture>(image.textureUUID);
            if (texture) {
                textureWidth = static_cast<float>(texture->GetWidth());
                textureHeight = static_cast<float>(texture->GetHeight());
                // Cache them for next time (mutable allows modification through const reference)
                if (textureWidth > 0.0f && textureHeight > 0.0f) {
                    const_cast<NE::ECS::Component::UIImage&>(image).cachedTextureWidth = textureWidth;
                    const_cast<NE::ECS::Component::UIImage&>(image).cachedTextureHeight = textureHeight;
                }
            }
        }

        switch (image.imageType) {
        case ImageType::SIMPLE:
            return GenerateSimple(x, y, z, width, height, color, 
                image.preserveAspect, textureWidth, textureHeight);

        case ImageType::SLICED:
            return GenerateSliced(image, x, y, z, width, height, color, textureWidth, textureHeight);

        case ImageType::TILED:
            return GenerateTiled(image, x, y, z, width, height, color);

        case ImageType::FILLED:
            return GenerateFilled(image, x, y, z, width, height, color,
                textureWidth, textureHeight);

        default:
            return GenerateSimple(x, y, z, width, height, color, 
                image.preserveAspect, textureWidth, textureHeight);
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
        const Math::Vec4& color,
        bool preserveAspect,
        float textureWidth,
        float textureHeight
    ) {
        float renderWidth = width;
        float renderHeight = height;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        
        // Preserve aspect ratio
        if (preserveAspect && textureWidth > 0.0f && textureHeight > 0.0f) {
            float textureAspect = textureWidth / textureHeight;
            float containerAspect = width / height;
            
            if (textureAspect > containerAspect) {
                renderWidth = width;
                renderHeight = width / textureAspect;
                offsetY = (height - renderHeight) * 0.5f;
            } else {
                renderHeight = height;
                renderWidth = height * textureAspect;
                offsetX = (width - renderWidth) * 0.5f;
            }
        }
        
        float finalX = x + offsetX;
        float finalY = y + offsetY;
        
        std::vector<UIVertex> vertices;
        vertices.reserve(6);

        vertices.push_back(CreateVertex(finalX, finalY, z, 0.0f, 0.0f, color));
        vertices.push_back(CreateVertex(finalX + renderWidth, finalY, z, 1.0f, 0.0f, color));
        vertices.push_back(CreateVertex(finalX + renderWidth, finalY + renderHeight, z, 1.0f, 1.0f, color));

        vertices.push_back(CreateVertex(finalX, finalY, z, 0.0f, 0.0f, color));
        vertices.push_back(CreateVertex(finalX + renderWidth, finalY + renderHeight, z, 1.0f, 1.0f, color));
        vertices.push_back(CreateVertex(finalX, finalY + renderHeight, z, 0.0f, 1.0f, color));

        return vertices;
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateSliced(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        float textureWidth,
        float textureHeight
    ) {
        // 9-slice: divide into 9 sections
        // Corners stay same size, edges stretch in one direction, center stretches both

        std::vector<UIVertex> vertices;
        vertices.reserve(54); // 9 quads � 6 vertices

        // Use texture dimensions if available, otherwise fallback to render dimensions
        float texW = textureWidth > 0.0f ? textureWidth : width;
        float texH = textureHeight > 0.0f ? textureHeight : height;

        // Border values are in texture pixels - convert to UV coordinates (0-1)
        float u0 = 0.0f;
        float u1 = image.borderLeft / texW;
        float u2 = (texW - image.borderRight) / texW;
        float u3 = 1.0f;

        float v0 = 0.0f;
        float v1 = image.borderBottom / texH;
        float v2 = (texH - image.borderTop) / texH;
        float v3 = 1.0f;

        // Calculate render positions (screen/world space)
        // Scale borders from texture pixels to render pixels
        float scaleX = width / texW;
        float scaleY = height / texH;

        float left = image.borderLeft * scaleX;
        float right = image.borderRight * scaleX;
        float top = image.borderTop * scaleY;
        float bottom = image.borderBottom * scaleY;

        // Prevent borders from being larger than the render size
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

        // Generate 9 quads
        AddQuad(x0, y0, x1, y1, u0, v0, u1, v1);
        AddQuad(x1, y0, x2, y1, u1, v0, u2, v1);
        AddQuad(x2, y0, x3, y1, u2, v0, u3, v1);
        AddQuad(x0, y1, x1, y2, u0, v1, u1, v2);
        AddQuad(x1, y1, x2, y2, u1, v1, u2, v2);
        AddQuad(x2, y1, x3, y2, u2, v1, u3, v2);
        AddQuad(x0, y2, x1, y3, u0, v2, u1, v3);
        AddQuad(x1, y2, x2, y3, u1, v2, u2, v3);
        AddQuad(x2, y2, x3, y3, u2, v2, u3, v3);

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

                vertices.push_back(CreateVertex(qx0, qy0, z, 0.0f, 0.0f, color));
                vertices.push_back(CreateVertex(qx1, qy0, z, qu1, 0.0f, color));
                vertices.push_back(CreateVertex(qx1, qy1, z, qu1, qv1, color));
                vertices.push_back(CreateVertex(qx0, qy0, z, 0.0f, 0.0f, color));
                vertices.push_back(CreateVertex(qx1, qy1, z, qu1, qv1, color));
                vertices.push_back(CreateVertex(qx0, qy1, z, 0.0f, qv1, color));
            }
        }

        return vertices;
    }

    std::vector<UIVertex> UIImageMeshGenerator::GenerateFilled(
        const NE::ECS::Component::UIImage& image,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color,
        float textureWidth,
        float textureHeight
    ) {
        using FillMethod = NE::ECS::Component::UIImage::FillMethod;
        
        float renderWidth = width;
        float renderHeight = height;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        
        // Preserve aspect ratio
        if (image.preserveAspect && textureWidth > 0.0f && textureHeight > 0.0f) {
            float textureAspect = textureWidth / textureHeight;
            float containerAspect = width / height;
            
            if (textureAspect > containerAspect) {
                renderWidth = width;
                renderHeight = width / textureAspect;
                offsetY = (height - renderHeight) * 0.5f;
            } else {
                renderHeight = height;
                renderWidth = height * textureAspect;
                offsetX = (width - renderWidth) * 0.5f;
            }
        }
        
        float finalX = x + offsetX;
        float finalY = y + offsetY;

        switch (image.fillMethod) {
        case FillMethod::HORIZONTAL:
            return GenerateHorizontalFill(image, finalX, finalY, z, renderWidth, renderHeight, color);

        case FillMethod::VERTICAL:
            return GenerateVerticalFill(image, finalX, finalY, z, renderWidth, renderHeight, color);

        case FillMethod::RADIAL_90:
        case FillMethod::RADIAL_180:
        case FillMethod::RADIAL_360:
            return GenerateRadialFill(image, finalX, finalY, z, renderWidth, renderHeight, color);

        default:
            return GenerateSimple(finalX, finalY, z, renderWidth, renderHeight, color, 
                image.preserveAspect, textureWidth, textureHeight);
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

        // Triangle 1 - Flip Y-axis: top screen maps to V=0, bottom screen maps to V=1
        vertices.push_back(CreateVertex(startX, y, z, u0, 0.0f, color));
        vertices.push_back(CreateVertex(endX, y, z, u1, 0.0f, color));
        vertices.push_back(CreateVertex(endX, y + height, z, u1, 1.0f, color));

        // Triangle 2
        vertices.push_back(CreateVertex(startX, y, z, u0, 0.0f, color));
        vertices.push_back(CreateVertex(endX, y + height, z, u1, 1.0f, color));
        vertices.push_back(CreateVertex(startX, y + height, z, u0, 1.0f, color));

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
        float v0 = 0.0f;
        float v1 = image.fillAmount;

        if (image.fillOrigin == FillOrigin::TOP) {
            startY = y + height - fillHeight;
            endY = y + height;
            v0 = 1.0f - image.fillAmount;
            v1 = 1.0f;
        }

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

            // Calculate UVs - Flip Y-axis: top screen maps to V=0, bottom screen maps to V=1
            float u1 = 0.5f + std::cos(angle1) * 0.5f;
            float v1 = 0.5f - std::sin(angle1) * 0.5f; // Flip Y: subtract instead of add
            float u2 = 0.5f + std::cos(angle2) * 0.5f;
            float v2 = 0.5f - std::sin(angle2) * 0.5f; // Flip Y: subtract instead of add

            // Create triangle from center
            vertices.push_back(CreateVertex(centerX, centerY, z, centerU, centerV, color));
            vertices.push_back(CreateVertex(x1, y1, z, u1, v1, color));
            vertices.push_back(CreateVertex(x2, y2, z, u2, v2, color));
        }

        return vertices;
    }

} // namespace NE::Graphics