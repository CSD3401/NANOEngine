#include "UITextMeshGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace NE::Graphics {

    UITextMeshGenerator::TextMeshResult UITextMeshGenerator::GenerateVertices(
        const std::string& text,
        const FontAtlas& fontAtlas,
        float x, float y, float z,
        float maxWidth, float maxHeight,
        const Math::Vec4& color,
        NE::ECS::Component::UIText::Alignment horizontalAlign,
        NE::ECS::Component::UIText::VerticalAlignment verticalAlign,
        bool wordWrap,
        float desiredFontSize,
        uint64_t bindlessHandle
    )
    {
        TextMeshResult result;
        result.totalWidth = 0.0f;
        result.totalHeight = 0.0f;

        if (text.empty()) {
            return result;
        }

        // Calculate scale factor if desired size differs from atlas size
        // This allows bucket-based caching where multiple sizes use the same atlas
        float scaleFactor = 1.0f;
        if (desiredFontSize > 0.0f && fontAtlas.GetFontSize() > 0.0f) {
            scaleFactor = desiredFontSize / fontAtlas.GetFontSize();
        }

        // Calculate lines with word wrapping (using scaled max width)
        std::vector<LineInfo> lines = CalculateLines(text, fontAtlas, maxWidth / scaleFactor, wordWrap);

        if (lines.empty()) {
            return result;
        }

        // Apply scale factor to font metrics
        float lineHeight = fontAtlas.GetLineHeight() * scaleFactor;
        float ascent = fontAtlas.GetAscent() * scaleFactor;
        float totalTextHeight = lines.size() * lineHeight;
        result.totalHeight = totalTextHeight;

        // Calculate max width for result (line widths are already scaled from CalculateLines)
        for (const auto& line : lines) {
            result.totalWidth = std::max(result.totalWidth, line.width * scaleFactor);
        }

        // Calculate starting Y position based on vertical alignment
        float startY = y;
        switch (verticalAlign) {
            case NE::ECS::Component::UIText::VerticalAlignment::TOP:
                startY = y + ascent;
                break;
            case NE::ECS::Component::UIText::VerticalAlignment::MIDDLE:
                startY = y + (maxHeight - totalTextHeight) * 0.5f + ascent;
                break;
            case NE::ECS::Component::UIText::VerticalAlignment::BOTTOM:
                startY = y + maxHeight - totalTextHeight + ascent;
                break;
        }

        // Generate vertices for each line
        float currentY = startY;
        for (const auto& line : lines) {
            // Calculate starting X position based on horizontal alignment
            float startX = x;
            switch (horizontalAlign) {
                case NE::ECS::Component::UIText::Alignment::LEFT:
                    startX = x;
                    break;
                case NE::ECS::Component::UIText::Alignment::CENTER:
                    startX = x + (maxWidth - line.width) * 0.5f;
                    break;
                case NE::ECS::Component::UIText::Alignment::RIGHT:
                    startX = x + maxWidth - line.width;
                    break;
            }

            GenerateLineVertices(result.vertices, line.text, fontAtlas, startX, currentY, z, color, scaleFactor, bindlessHandle);
            currentY += lineHeight;
        }

        return result;
    }

    std::vector<UITextMeshGenerator::LineInfo> UITextMeshGenerator::CalculateLines(
        const std::string& text,
        const FontAtlas& fontAtlas,
        float maxWidth,
        bool wordWrap
    ) 
    {
        std::vector<LineInfo> lines;

        if (text.empty()) {
            return lines;
        }

        std::string currentLine;
        float currentWidth = 0.0f;
        std::string currentWord;
        float currentWordWidth = 0.0f;

        auto finishLine = [&]() {
            if (!currentLine.empty()) {
                // Trim trailing spaces
                while (!currentLine.empty() && currentLine.back() == ' ') {
                    currentLine.pop_back();
                    currentWidth = CalculateLineWidth(currentLine, fontAtlas);
                }
                LineInfo lineInfo;
                lineInfo.text = currentLine;
                lineInfo.width = currentWidth;
                lines.push_back(lineInfo);
            }
            currentLine.clear();
            currentWidth = 0.0f;
        };

        auto addWordToLine = [&]() {
            if (currentLine.empty()) {
                currentLine = currentWord;
                currentWidth = currentWordWidth;
            } else {
                currentLine += currentWord;
                currentWidth += currentWordWidth;
            }
            currentWord.clear();
            currentWordWidth = 0.0f;
        };

        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];

            // Handle newlines
            if (c == '\n') {
                addWordToLine();
                finishLine();
                continue;
            }

            const GlyphInfo* glyph = fontAtlas.GetGlyph(c);
            if (!glyph) continue;

            float charWidth = glyph->xAdvance;

            if (c == ' ') {
                // End of word
                if (wordWrap && currentWidth + currentWordWidth + charWidth > maxWidth && !currentLine.empty()) {
                    // Word doesn't fit on current line, start new line
                    finishLine();
                    addWordToLine();
                } else {
                    addWordToLine();
                }
                currentWord += c;
                currentWordWidth = charWidth;
                addWordToLine();
            } else {
                // Continue building word
                currentWord += c;
                currentWordWidth += charWidth;

                // If word wrap is disabled and we exceed width, just keep going
                if (!wordWrap) {
                    continue;
                }

                // Check if current word alone is too long
                if (currentWordWidth > maxWidth && currentLine.empty()) {
                    // Single word is too long, break it
                    addWordToLine();
                    finishLine();
                }
            }
        }

        // Add remaining word and line
        addWordToLine();
        finishLine();

        return lines;
    }

    float UITextMeshGenerator::CalculateLineWidth(
        const std::string& line,
        const FontAtlas& fontAtlas
    ) 
    {
        float width = 0.0f;
        for (char c : line) {
            const GlyphInfo* glyph = fontAtlas.GetGlyph(c);
            if (glyph) {
                width += glyph->xAdvance;
            }
        }
        return width;
    }

    void UITextMeshGenerator::GenerateLineVertices(
        std::vector<UIVertex2>& vertices,
        const std::string& line,
        const FontAtlas& fontAtlas,
        float startX, float startY, float z,
        const Math::Vec4& color,
        float scaleFactor,
        uint64_t bindlessHandle
    )
    {
        float cursorX = startX;

        for (char c : line) {
            const GlyphInfo* glyph = fontAtlas.GetGlyph(c);
            if (!glyph || glyph->width <= 0 || glyph->height <= 0) {
                if (glyph) {
                    cursorX += glyph->xAdvance * scaleFactor;
                }
                continue;
            }

            // Calculate glyph quad position (apply scale factor to all metrics)
            float x0 = cursorX + glyph->xOffset * scaleFactor;
            float y0 = startY + glyph->yOffset * scaleFactor;
            float x1 = x0 + glyph->width * scaleFactor;
            float y1 = y0 + glyph->height * scaleFactor;

            // Create two triangles (6 vertices) for the glyph quad
            // Triangle 1: top-left, bottom-left, bottom-right
            vertices.push_back(CreateVertex(x0, y0, z, glyph->u0, glyph->v0, color, bindlessHandle)); // top-left
            vertices.push_back(CreateVertex(x0, y1, z, glyph->u0, glyph->v1, color, bindlessHandle)); // bottom-left
            vertices.push_back(CreateVertex(x1, y1, z, glyph->u1, glyph->v1, color, bindlessHandle)); // bottom-right

            // Triangle 2: top-left, bottom-right, top-right
            vertices.push_back(CreateVertex(x0, y0, z, glyph->u0, glyph->v0, color, bindlessHandle)); // top-left
            vertices.push_back(CreateVertex(x1, y1, z, glyph->u1, glyph->v1, color, bindlessHandle)); // bottom-right
            vertices.push_back(CreateVertex(x1, y0, z, glyph->u1, glyph->v0, color, bindlessHandle)); // top-right

            cursorX += glyph->xAdvance * scaleFactor;
        }
    }

    UIVertex2 UITextMeshGenerator::CreateVertex(
        float x, float y, float z,
        float u, float v,
        const Math::Vec4& color,
        uint64_t bindlessHandle
    )
    {
        uint32_t handleLo = static_cast<uint32_t>(bindlessHandle & 0xFFFFFFFF);
        uint32_t handleHi = static_cast<uint32_t>(bindlessHandle >> 32);
        UIVertex2 vertex;
        vertex.Position = Math::Vec3(x, y, z);
        vertex.TexCoord = Math::Vec2(u, v);
        vertex.Color = color;
        vertex.texHandleLo = handleLo;
        vertex.texHandleHi = handleHi;
        return vertex;
    }

    float UITextMeshGenerator::CalculateFitFontSize(
        const std::string& text,
        const FontAtlas& fontAtlas,
        float maxWidth,
        float maxHeight,
        float baseFontSize,
        float minFontSize,
        float maxFontSize,
        bool wordWrap
    )
    {
        if (text.empty() || maxWidth <= 0.0f || maxHeight <= 0.0f) {
            return baseFontSize;
        }

        // Start with base font size clamped to range
        float currentFontSize = std::clamp(baseFontSize, minFontSize, maxFontSize);

        // Calculate scale factor relative to atlas font size
        float atlasSize = fontAtlas.GetFontSize();
        if (atlasSize <= 0.0f) {
            return currentFontSize;
        }

        // Binary search for optimal font size
        float low = minFontSize;
        float high = maxFontSize;
        float bestFit = currentFontSize;
        const int maxIterations = 10; // Limit iterations for performance
        const float tolerance = 0.5f; // Good enough if within 0.5 pixels

        for (int i = 0; i < maxIterations; ++i) {
            float testSize = (low + high) / 2.0f;
            float scaleFactor = testSize / atlasSize;

            // Calculate text dimensions at this size
            std::vector<LineInfo> lines = CalculateLines(text, fontAtlas, maxWidth / scaleFactor, wordWrap);

            if (lines.empty()) {
                high = testSize;
                continue;
            }

            // Calculate total text dimensions
            float textHeight = lines.size() * fontAtlas.GetLineHeight() * scaleFactor;
            float textWidth = 0.0f;
            for (const auto& line : lines) {
                textWidth = std::max(textWidth, line.width * scaleFactor);
            }

            // Check if text fits
            bool fitsWidth = textWidth <= maxWidth + tolerance;
            bool fitsHeight = textHeight <= maxHeight + tolerance;

            if (fitsWidth && fitsHeight) {
                // Text fits! Try larger
                bestFit = testSize;
                low = testSize;

                // If we're very close to max size, stop
                if (high - low < tolerance) {
                    break;
                }
            } else {
                // Text doesn't fit, try smaller
                high = testSize;
            }
        }

        return bestFit;
    }

} // namespace NE::Graphics
