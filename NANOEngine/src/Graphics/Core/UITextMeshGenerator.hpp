#ifndef UI_TEXT_MESH_GENERATOR_HPP
#define UI_TEXT_MESH_GENERATOR_HPP

#include <vector>
#include <string>
#include <memory>
#include "UIImageMeshGenerator.hpp"
#include "FontAtlas.hpp"
#include "ECS/Components/UIText.hpp"
#include "../../Math/Vec4.hpp"

namespace NE::Graphics {

    class UITextMeshGenerator {
    public:
        struct TextMeshResult {
            std::vector<UIVertex2> vertices;
            float totalWidth;
            float totalHeight;
        };

        /// Per-character style produced by ParseRichText.
        struct CharStyle {
            NE::Math::Vec4 color;
            float sizeScale = 1.0f;  ///< Multiplier relative to base font size (1.0 = no change)
        };

        /// Strip rich text tags from \p richText and populate per-character style data.
        /// Supported tags: <color=#RRGGBB[AA]> </color>  <size=N> </size>
        ///                 <b> </b>  <i> </i>  (bold/italic parsed but visually ignored)
        /// Unknown tags are emitted as literal text.
        static void ParseRichText(
            const std::string& richText,
            const NE::Math::Vec4& baseColor,
            float baseFontSize,
            std::string& outStripped,
            std::vector<CharStyle>& outStyles
        );

        static TextMeshResult GenerateVertices(
            const std::string& text,
            const FontAtlas& fontAtlas,
            float x, float y, float z,
            float maxWidth,
            float maxHeight,
            const Math::Vec4& color,
            NE::ECS::Component::UIText::Alignment horizontalAlign,
            NE::ECS::Component::UIText::VerticalAlignment verticalAlign,
            bool wordWrap,
            float desiredFontSize = 0.0f,
            uint64_t bindlessHandle = 0,
            float lineSpacing = 1.0f,
            const std::vector<CharStyle>* charStyles = nullptr
        );

        // Calculate font size to fit within bounds (for auto-scaling)
        static float CalculateFitFontSize(
            const std::string& text,
            const FontAtlas& fontAtlas,
            float maxWidth,
            float maxHeight,
            float baseFontSize,
            float minFontSize,
            float maxFontSize,
            bool wordWrap
        );

    private:
        struct LineInfo {
            std::string text;
            float width;           ///< Sum of (xAdvance * sizeScale) for each char — atlas-space * relative scale
            size_t startIndex;     ///< Index of first char in the full stripped string
            size_t endIndex;       ///< One-past index of last char
            float maxSizeScale;    ///< Max sizeScale of any char in this line (drives line height)
        };

        static std::vector<LineInfo> CalculateLines(
            const std::string& text,
            const FontAtlas& fontAtlas,
            float maxWidth,
            bool wordWrap,
            const std::vector<CharStyle>* charStyles = nullptr
        );

        static float CalculateLineWidth(
            const std::string& line,
            const FontAtlas& fontAtlas
        );

        static void GenerateLineVertices(
            std::vector<UIVertex2>& vertices,
            const std::string& line,
            const FontAtlas& fontAtlas,
            float startX, float startY, float z,
            const Math::Vec4& color,
            float scaleFactor = 1.0f,
            uint64_t bindlessHandle = 0,
            size_t lineStartCharIdx = 0,
            const std::vector<CharStyle>* charStyles = nullptr
        );

        static UIVertex2 CreateVertex(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color,
            uint64_t bindlessHandle = 0
        );
    };

} // namespace NE::Graphics

#endif // UI_TEXT_MESH_GENERATOR_HPP
