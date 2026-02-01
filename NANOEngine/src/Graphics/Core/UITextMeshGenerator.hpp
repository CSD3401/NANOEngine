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
            std::vector<UIVertex> vertices;
            float totalWidth;
            float totalHeight;
        };

        static TextMeshResult GenerateVertices(
            const std::string& text,
            const FontAtlas& fontAtlas,
            float x, float y, float z,
            float maxWidth,
            float maxHeight,
            const Math::Vec4& color,
            NE::ECS::Component::UIText::Alignment horizontalAlign,
            NE::ECS::Component::UIText::VerticalAlignment verticalAlign,
            bool wordWrap
        );

    private:
        struct LineInfo {
            std::string text;
            float width;
            size_t startIndex;
            size_t endIndex;
        };

        static std::vector<LineInfo> CalculateLines(
            const std::string& text,
            const FontAtlas& fontAtlas,
            float maxWidth,
            bool wordWrap
        );

        static float CalculateLineWidth(
            const std::string& line,
            const FontAtlas& fontAtlas
        );

        static void GenerateLineVertices(
            std::vector<UIVertex>& vertices,
            const std::string& line,
            const FontAtlas& fontAtlas,
            float startX, float startY, float z,
            const Math::Vec4& color
        );

        static UIVertex CreateVertex(
            float x, float y, float z,
            float u, float v,
            const Math::Vec4& color
        );
    };

} // namespace NE::Graphics

#endif // UI_TEXT_MESH_GENERATOR_HPP
