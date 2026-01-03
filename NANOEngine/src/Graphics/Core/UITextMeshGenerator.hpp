#ifndef UI_TEXT_MESH_GENERATOR_HPP
#define UI_TEXT_MESH_GENERATOR_HPP

#include <vector>
#include <string>
#include "ECS/Components/UIText.hpp"
#include "Font.hpp"
#include "UIImageMeshGenerator.hpp"  // For UIVertex definition
#include "../../Math/Vec2.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::Graphics {

    class UITextMeshGenerator {
    public:
        // Generate vertices for text rendering
        static std::vector<UIVertex> GenerateVertices(
            const NE::ECS::Component::UIText& text,
            const Font& font,
            float x, float y, float z,
            float width, float height,
            const Math::Vec4& color
        );

    private:
        // Helper to create a single vertex
        static UIVertex CreateVertex(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color
        );

        // Generate vertices for a single character
        static void GenerateCharacterQuad(
            std::vector<UIVertex>& vertices,
            const GlyphMetrics& metrics,
            float x, float y, float z,
            const Math::Vec4& color,
            float normalizedAscender
        );

        // Generate vertices for a single character with horizontal clipping
        static void GenerateCharacterQuadClipped(
            std::vector<UIVertex>& vertices,
            const GlyphMetrics& metrics,
            float x, float y, float z,
            const Math::Vec4& color,
            float clipLeft,
            float clipRight,
            float normalizedAscender
        );

        // Calculate text layout (word wrapping, alignment)
        struct TextLine {
            std::string text;
            float width;
            bool isParagraphStart = false; // True if this is the first line of a paragraph
        };
        static std::vector<TextLine> CalculateTextLines(
            const std::string& text,
            const Font& font,
            float maxWidth,
            bool wordWrap
        );

        // Calculate horizontal offset for alignment
        static float CalculateHorizontalOffset(
            const TextLine& line,
            const Font& font,
            float containerWidth,
            NE::ECS::Component::UIText::Alignment alignment
        );

        // Calculate vertical offset for alignment
        static float CalculateVerticalOffset(
            const std::vector<TextLine>& lines,
            const Font& font,
            float containerHeight,
            NE::ECS::Component::UIText::VerticalAlignment alignment,
            float lineSpacing = 1.0f
        );
    };

} // namespace NE::Graphics

#endif // UI_TEXT_MESH_GENERATOR_HPP

