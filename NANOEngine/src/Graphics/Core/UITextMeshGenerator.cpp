#include "UITextMeshGenerator.hpp"
#include "Core/SpdLogger.hpp"
#include <algorithm>
#include <cmath>

namespace NE::Graphics {

    std::vector<UIVertex> UITextMeshGenerator::GenerateVertices(
        const NE::ECS::Component::UIText& text,
        const Font& font,
        float x, float y, float z,
        float width, float height,
        const Math::Vec4& color
    ) {
        if (text.text.empty()) {
            return {};
        }

        std::vector<UIVertex> vertices;

        // Calculate text lines (with word wrapping if enabled)
        float maxWidth = (text.wordWrap && width > 0.0f) ? width : 0.0f;
        std::vector<TextLine> lines = CalculateTextLines(text.text, font, maxWidth, text.wordWrap);

        if (lines.empty()) {
            return {};
        }

        // Calculate vertical alignment offset
        float totalTextHeight = static_cast<float>(lines.size()) * font.GetLineHeight();
        float verticalOffset = CalculateVerticalOffset(lines, font, height, text.verticalAlign);

        // Generate vertices for each line
        // Start from top (y is top-left in top-down coordinate system)
        // Add baseline offset to position text correctly
        float baselineOffset = font.GetAscender();
        float currentY = y + verticalOffset + baselineOffset;

        for (const auto& line : lines) {
            // Calculate horizontal alignment offset
            float horizontalOffset = CalculateHorizontalOffset(line, font, width, text.horizontalAlign);

            float currentX = x + horizontalOffset;

            // Generate quads for each character in the line
            for (size_t i = 0; i < line.text.length(); ++i) {
                uint32_t codepoint = static_cast<unsigned char>(line.text[i]);

                // Skip control characters except newline (already handled in CalculateTextLines)
                if (codepoint < 32 && codepoint != '\n') {
                    continue;
                }

                const GlyphMetrics* metrics = font.GetGlyphMetrics(codepoint);
                if (!metrics) {
                    // Use space width as fallback
                    const GlyphMetrics* spaceMetrics = font.GetGlyphMetrics(' ');
                    if (spaceMetrics) {
                        currentX += spaceMetrics->advanceX;
                    }
                    continue;
                }

                // Generate quad for this character
                GenerateCharacterQuad(vertices, *metrics, currentX, currentY, z, color);

                // Advance cursor
                currentX += metrics->advanceX;

                // Add kerning if next character exists
                if (i + 1 < line.text.length()) {
                    uint32_t nextCodepoint = static_cast<unsigned char>(line.text[i + 1]);
                    currentX += font.GetKerning(codepoint, nextCodepoint);
                }

                // Add character spacing
                currentX += text.characterSpacing;
            }

            // Move to next line
            currentY += font.GetLineHeight() * text.lineSpacing;
        }

        return vertices;
    }

    UIVertex UITextMeshGenerator::CreateVertex(
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

    void UITextMeshGenerator::GenerateCharacterQuad(
        std::vector<UIVertex>& vertices,
        const GlyphMetrics& metrics,
        float x, float y, float z,
        const Math::Vec4& color
    ) {
        // Calculate character position
        // bearingX: horizontal offset from cursor (can be negative for some characters)
        // For Y: in top-down system, we position from baseline
        // bearingY from stb_truetype is the offset from baseline (positive = above baseline)
        // In top-down: baseline is at y, so we subtract bearingY to get top of character
        float charX = x + metrics.bearingX;
        float charY = y - metrics.bearingY; // bearingY is distance from baseline upward

        // Character quad dimensions
        float charWidth = metrics.width;
        float charHeight = metrics.height;

        // Generate quad (2 triangles, 6 vertices)
        // Top-left origin system (Y-down)
        // UV coordinates from font atlas
        vertices.push_back(CreateVertex(
            charX, charY, z,
            metrics.u0, metrics.v1, // Top-left of glyph in atlas
            color
        ));

        vertices.push_back(CreateVertex(
            charX + charWidth, charY, z,
            metrics.u1, metrics.v1, // Top-right of glyph in atlas
            color
        ));

        vertices.push_back(CreateVertex(
            charX + charWidth, charY + charHeight, z,
            metrics.u1, metrics.v0, // Bottom-right of glyph in atlas
            color
        ));

        // Second triangle
        vertices.push_back(CreateVertex(
            charX, charY, z,
            metrics.u0, metrics.v1, // Top-left
            color
        ));

        vertices.push_back(CreateVertex(
            charX + charWidth, charY + charHeight, z,
            metrics.u1, metrics.v0, // Bottom-right
            color
        ));

        vertices.push_back(CreateVertex(
            charX, charY + charHeight, z,
            metrics.u0, metrics.v0, // Bottom-left of glyph in atlas
            color
        ));
    }

    std::vector<UITextMeshGenerator::TextLine> UITextMeshGenerator::CalculateTextLines(
        const std::string& text,
        const Font& font,
        float maxWidth,
        bool wordWrap
    ) {
        std::vector<TextLine> lines;

        if (text.empty()) {
            return lines;
        }

        // Split by newlines first
        std::vector<std::string> paragraphs;
        std::string currentParagraph;
        for (char c : text) {
            if (c == '\n') {
                if (!currentParagraph.empty()) {
                    paragraphs.push_back(currentParagraph);
                    currentParagraph.clear();
                } else {
                    paragraphs.push_back(""); // Empty line
                }
            } else {
                currentParagraph += c;
            }
        }
        if (!currentParagraph.empty()) {
            paragraphs.push_back(currentParagraph);
        }

        // Process each paragraph
        for (const auto& paragraph : paragraphs) {
            if (paragraph.empty()) {
                lines.push_back({ "", 0.0f });
                continue;
            }

            if (!wordWrap || maxWidth <= 0.0f) {
                // No wrapping - single line
                float lineWidth = font.MeasureTextWidth(paragraph);
                lines.push_back({ paragraph, lineWidth });
            } else {
                // Word wrapping
                std::string currentLine;
                float currentLineWidth = 0.0f;

                // Split into words
                std::vector<std::string> words;
                std::string currentWord;
                for (char c : paragraph) {
                    if (c == ' ' || c == '\t') {
                        if (!currentWord.empty()) {
                            words.push_back(currentWord);
                            currentWord.clear();
                        }
                        // Add space as a word
                        words.push_back(std::string(1, c));
                    } else {
                        currentWord += c;
                    }
                }
                if (!currentWord.empty()) {
                    words.push_back(currentWord);
                }

                // Build lines
                for (const auto& word : words) {
                    float wordWidth = font.MeasureTextWidth(word);

                    if (currentLine.empty()) {
                        // First word on line
                        currentLine = word;
                        currentLineWidth = wordWidth;
                    } else {
                        // Check if word fits on current line
                        float spaceWidth = font.MeasureTextWidth(" ");
                        float totalWidth = currentLineWidth + spaceWidth + wordWidth;

                        if (totalWidth <= maxWidth) {
                            // Add to current line
                            currentLine += word;
                            currentLineWidth = totalWidth;
                        } else {
                            // Start new line
                            lines.push_back({ currentLine, currentLineWidth });
                            currentLine = word;
                            currentLineWidth = wordWidth;
                        }
                    }
                }

                // Add last line
                if (!currentLine.empty()) {
                    lines.push_back({ currentLine, currentLineWidth });
                }
            }
        }

        return lines;
    }

    float UITextMeshGenerator::CalculateHorizontalOffset(
        const TextLine& line,
        const Font& font,
        float containerWidth,
        NE::ECS::Component::UIText::Alignment alignment
    ) {
        if (containerWidth <= 0.0f) {
            return 0.0f;
        }

        float textWidth = line.width;

        switch (alignment) {
        case NE::ECS::Component::UIText::Alignment::LEFT:
            return 0.0f;

        case NE::ECS::Component::UIText::Alignment::CENTER:
            return (containerWidth - textWidth) * 0.5f;

        case NE::ECS::Component::UIText::Alignment::RIGHT:
            return containerWidth - textWidth;

        case NE::ECS::Component::UIText::Alignment::JUSTIFY:
            // For now, treat justify as left
            // TODO: Implement proper justification
            return 0.0f;

        default:
            return 0.0f;
        }
    }

    float UITextMeshGenerator::CalculateVerticalOffset(
        const std::vector<TextLine>& lines,
        const Font& font,
        float containerHeight,
        NE::ECS::Component::UIText::VerticalAlignment alignment
    ) {
        if (containerHeight <= 0.0f || lines.empty()) {
            return 0.0f;
        }

        float totalTextHeight = static_cast<float>(lines.size()) * font.GetLineHeight();

        switch (alignment) {
        case NE::ECS::Component::UIText::VerticalAlignment::TOP:
            return 0.0f;

        case NE::ECS::Component::UIText::VerticalAlignment::MIDDLE:
            return (containerHeight - totalTextHeight) * 0.5f;

        case NE::ECS::Component::UIText::VerticalAlignment::BOTTOM:
            return containerHeight - totalTextHeight;

        default:
            return 0.0f;
        }
    }

} // namespace NE::Graphics

