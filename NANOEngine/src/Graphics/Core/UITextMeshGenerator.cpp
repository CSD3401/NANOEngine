#include "pch.h"
#include "UITextMeshGenerator.hpp"
#include <algorithm>
#include <cmath>

namespace NE::Graphics {

    // =========================================================================
    // Rich text parser
    // =========================================================================

    static unsigned ParseHexNibble(char c)
    {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A' + 10);
        return 0;
    }

    static float ParseHexByte(const std::string& s, size_t pos)
    {
        if (pos + 1 >= s.size()) return 0.0f;
        return static_cast<float>(ParseHexNibble(s[pos]) * 16 + ParseHexNibble(s[pos + 1])) / 255.0f;
    }

    void UITextMeshGenerator::ParseRichText(
        const std::string& richText,
        const NE::Math::Vec4& baseColor,
        float baseFontSize,
        std::string& outStripped,
        std::vector<CharStyle>& outStyles)
    {
        outStripped.clear();
        outStyles.clear();

        // State stacks (always have at least one entry = base style)
        std::vector<NE::Math::Vec4> colorStack{ baseColor };
        std::vector<float>          sizeStack{ 1.0f };

        size_t i = 0;
        while (i < richText.size()) {
            if (richText[i] != '<') {
                outStripped += richText[i];
                outStyles.push_back({ colorStack.back(), sizeStack.back() });
                ++i;
                continue;
            }

            // Locate closing '>'
            size_t end = richText.find('>', i);
            if (end == std::string::npos) {
                // Malformed — emit literal '<'
                outStripped += '<';
                outStyles.push_back({ colorStack.back(), sizeStack.back() });
                ++i;
                continue;
            }

            std::string tag = richText.substr(i + 1, end - i - 1);
            i = end + 1;

            // --- Closing tags ---
            if (tag == "/color" || tag == "/Color") {
                if (colorStack.size() > 1) colorStack.pop_back();
                continue;
            }
            if (tag == "/size" || tag == "/Size") {
                if (sizeStack.size() > 1) sizeStack.pop_back();
                continue;
            }
            // Bold/Italic: parsed and ignored (no font variant support)
            if (tag == "b" || tag == "/b" || tag == "i" || tag == "/i" ||
                tag == "B" || tag == "/B" || tag == "I" || tag == "/I") {
                continue;
            }

            // --- Opening tags ---
            // color=#RRGGBB or color=#RRGGBBAA
            if (tag.size() >= 5 &&
                (tag.substr(0, 5) == "color" || tag.substr(0, 5) == "Color"))
            {
                size_t eq = tag.find('=');
                NE::Math::Vec4 newColor = colorStack.back();
                if (eq != std::string::npos && eq + 2 < tag.size() && tag[eq + 1] == '#') {
                    const std::string hex = tag.substr(eq + 2);
                    if (hex.size() >= 6) {
                        newColor.x = ParseHexByte(hex, 0);  // R
                        newColor.y = ParseHexByte(hex, 2);  // G
                        newColor.z = ParseHexByte(hex, 4);  // B
                        newColor.w = (hex.size() >= 8) ? ParseHexByte(hex, 6) : colorStack.back().w;
                    }
                }
                colorStack.push_back(newColor);
                continue;
            }

            // size=N  (absolute pixel size)
            if (tag.size() >= 4 &&
                (tag.substr(0, 4) == "size" || tag.substr(0, 4) == "Size"))
            {
                size_t eq = tag.find('=');
                float newScale = sizeStack.back();
                if (eq != std::string::npos && eq + 1 < tag.size()) {
                    try {
                        float newSize = std::stof(tag.substr(eq + 1));
                        if (baseFontSize > 0.0f && newSize > 0.0f)
                            newScale = newSize / baseFontSize;
                    } catch (...) {}
                }
                sizeStack.push_back(newScale);
                continue;
            }

            // Unknown tag — emit as literal text
            {
                std::string literal = "<" + tag + ">";
                CharStyle s{ colorStack.back(), sizeStack.back() };
                for (char c : literal) {
                    outStripped += c;
                    outStyles.push_back(s);
                }
            }
        }
    }

    // =========================================================================
    // CalculateLines
    // =========================================================================

    std::vector<UITextMeshGenerator::LineInfo> UITextMeshGenerator::CalculateLines(
        const std::string& text,
        const FontAtlas& fontAtlas,
        float maxWidth,
        bool wordWrap,
        const std::vector<CharStyle>* charStyles)
    {
        std::vector<LineInfo> lines;
        if (text.empty()) return lines;

        auto getScale = [&](size_t idx) -> float {
            if (!charStyles || idx >= charStyles->size()) return 1.0f;
            return (*charStyles)[idx].sizeScale;
        };

        auto glyphAdvance = [&](char c, size_t idx) -> float {
            const GlyphInfo* g = fontAtlas.GetGlyph(c);
            return g ? g->xAdvance * getScale(idx) : 0.0f;
        };

        // --- Current line state ---
        std::string line;
        float       lineWidth    = 0.0f;
        size_t      lineStart    = 0;
        float       lineMaxScale = 1.0f;

        // --- Pending word state ---
        std::string word;
        float       wordWidth    = 0.0f;
        size_t      wordStart    = 0;
        float       wordMaxScale = 1.0f;

        // Commit the current line to the output list
        auto commitLine = [&]() {
            // Trim trailing spaces from right
            while (!line.empty() && line.back() == ' ') {
                size_t spIdx = lineStart + line.size() - 1;
                lineWidth -= glyphAdvance(' ', spIdx);
                if (lineWidth < 0.0f) lineWidth = 0.0f;
                line.pop_back();
            }
            if (!line.empty()) {
                LineInfo li;
                li.text        = std::move(line);
                li.width       = lineWidth;
                li.startIndex  = lineStart;
                li.endIndex    = lineStart + li.text.size();
                li.maxSizeScale = lineMaxScale;
                lines.push_back(std::move(li));
            }
            line.clear();
            lineWidth    = 0.0f;
            lineMaxScale = 1.0f;
            // lineStart is updated by addToLine / addSpaceToLine when next content arrives
        };

        // Append the pending word to the current line
        auto flushWord = [&]() {
            if (word.empty()) return;
            if (line.empty()) lineStart = wordStart;
            line     += word;
            lineWidth += wordWidth;
            lineMaxScale = std::max(lineMaxScale, wordMaxScale);
            word.clear();
            wordWidth    = 0.0f;
            wordMaxScale = 1.0f;
        };

        for (size_t i = 0; i < text.size(); ++i) {
            char c = text[i];

            if (c == '\n') {
                flushWord();
                commitLine();
                lineStart = i + 1;
                continue;
            }

            const GlyphInfo* glyph = fontAtlas.GetGlyph(c);
            if (!glyph) continue;

            float cw = glyph->xAdvance * getScale(i);

            if (c == ' ') {
                // Check if pending word + this space would exceed max width
                if (wordWrap && !word.empty() && !line.empty() &&
                    lineWidth + wordWidth + cw > maxWidth)
                {
                    // Move pending word to new line
                    size_t savedWordStart = wordStart;
                    commitLine();
                    lineStart = savedWordStart;
                    flushWord();
                }
                else {
                    flushWord();
                }
                // Add space directly to the current line
                if (line.empty()) lineStart = i;
                line     += ' ';
                lineWidth += cw;
                lineMaxScale = std::max(lineMaxScale, getScale(i));
            }
            else {
                // Non-space: accumulate into pending word
                if (word.empty()) wordStart = i;
                word     += c;
                wordWidth += cw;
                wordMaxScale = std::max(wordMaxScale, getScale(i));

                if (!wordWrap) continue;

                // Word alone exceeds line width — force-break it
                if (wordWidth > maxWidth && line.empty()) {
                    flushWord();
                    commitLine();
                    lineStart = i + 1;
                }
            }
        }

        // Flush remaining word and line
        flushWord();
        commitLine();

        return lines;
    }

    // =========================================================================
    // GenerateVertices
    // =========================================================================

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
        uint64_t bindlessHandle,
        float lineSpacing,
        const std::vector<CharStyle>* charStyles)
    {
        TextMeshResult result;
        result.totalWidth  = 0.0f;
        result.totalHeight = 0.0f;

        if (text.empty()) return result;

        // Base scale factor: desired size relative to atlas bucket size
        float scaleFactor = 1.0f;
        if (desiredFontSize > 0.0f && fontAtlas.GetFontSize() > 0.0f)
            scaleFactor = desiredFontSize / fontAtlas.GetFontSize();

        // CalculateLines works in atlas-space units (* sizeScale), so pass maxWidth / scaleFactor
        std::vector<LineInfo> lines = CalculateLines(text, fontAtlas, maxWidth / scaleFactor, wordWrap, charStyles);
        if (lines.empty()) return result;

        const float baseLineHeight = fontAtlas.GetLineHeight() * scaleFactor;
        const float ascent         = fontAtlas.GetAscent()     * scaleFactor;

        // Per-line height accounts for the largest sizeScale on that line
        // totalTextHeight is the sum of all line heights (with spacing)
        float totalTextHeight = 0.0f;
        for (const auto& li : lines) {
            totalTextHeight += baseLineHeight * li.maxSizeScale * lineSpacing;
        }

        // Widest line (scaled to screen space) for totalWidth reporting
        for (const auto& li : lines) {
            result.totalWidth = std::max(result.totalWidth, li.width * scaleFactor);
        }
        result.totalHeight = totalTextHeight;

        // Vertical start position
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

        float currentY = startY;
        for (const auto& li : lines) {
            float thisLineHeight  = baseLineHeight * li.maxSizeScale * lineSpacing;
            float scaledLineWidth = li.width * scaleFactor;

            float startX = x;
            switch (horizontalAlign) {
            case NE::ECS::Component::UIText::Alignment::LEFT:
                startX = x;
                break;
            case NE::ECS::Component::UIText::Alignment::CENTER:
                startX = x + (maxWidth - scaledLineWidth) * 0.5f;
                break;
            case NE::ECS::Component::UIText::Alignment::RIGHT:
                startX = x + maxWidth - scaledLineWidth;
                break;
            }

            GenerateLineVertices(result.vertices, li.text, fontAtlas,
                startX, currentY, z,
                color, scaleFactor, bindlessHandle,
                li.startIndex, charStyles);

            currentY += thisLineHeight;
        }

        return result;
    }

    // =========================================================================
    // CalculateLineWidth  (unchanged — used by CalculateFitFontSize)
    // =========================================================================

    float UITextMeshGenerator::CalculateLineWidth(
        const std::string& line,
        const FontAtlas& fontAtlas)
    {
        float width = 0.0f;
        for (char c : line) {
            const GlyphInfo* glyph = fontAtlas.GetGlyph(c);
            if (glyph) width += glyph->xAdvance;
        }
        return width;
    }

    // =========================================================================
    // GenerateLineVertices
    // =========================================================================

    void UITextMeshGenerator::GenerateLineVertices(
        std::vector<UIVertex2>& vertices,
        const std::string& line,
        const FontAtlas& fontAtlas,
        float startX, float startY, float z,
        const Math::Vec4& color,
        float scaleFactor,
        uint64_t bindlessHandle,
        size_t lineStartCharIdx,
        const std::vector<CharStyle>* charStyles)
    {
        float cursorX = startX;

        for (size_t j = 0; j < line.size(); ++j) {
            char c = line[j];
            const GlyphInfo* glyph = fontAtlas.GetGlyph(c);

            // Per-character overrides from rich text
            Math::Vec4 charColor = color;
            float      charScale = scaleFactor;
            if (charStyles) {
                size_t idx = lineStartCharIdx + j;
                if (idx < charStyles->size()) {
                    charColor = (*charStyles)[idx].color;
                    charScale = scaleFactor * (*charStyles)[idx].sizeScale;
                }
            }

            if (!glyph || glyph->width <= 0 || glyph->height <= 0) {
                if (glyph) cursorX += glyph->xAdvance * charScale;
                continue;
            }

            float x0 = cursorX + glyph->xOffset * charScale;
            float y0 = startY  + glyph->yOffset * charScale;
            float x1 = x0 + glyph->width  * charScale;
            float y1 = y0 + glyph->height * charScale;

            vertices.push_back(CreateVertex(x0, y0, z, glyph->u0, glyph->v0, charColor, bindlessHandle));
            vertices.push_back(CreateVertex(x0, y1, z, glyph->u0, glyph->v1, charColor, bindlessHandle));
            vertices.push_back(CreateVertex(x1, y1, z, glyph->u1, glyph->v1, charColor, bindlessHandle));
            vertices.push_back(CreateVertex(x0, y0, z, glyph->u0, glyph->v0, charColor, bindlessHandle));
            vertices.push_back(CreateVertex(x1, y1, z, glyph->u1, glyph->v1, charColor, bindlessHandle));
            vertices.push_back(CreateVertex(x1, y0, z, glyph->u1, glyph->v0, charColor, bindlessHandle));

            cursorX += glyph->xAdvance * charScale;
        }
    }

    // =========================================================================
    // CreateVertex
    // =========================================================================

    UIVertex2 UITextMeshGenerator::CreateVertex(
        float x, float y, float z,
        float u, float v,
        const Math::Vec4& color,
        uint64_t bindlessHandle)
    {
        uint32_t handleLo = static_cast<uint32_t>(bindlessHandle & 0xFFFFFFFF);
        uint32_t handleHi = static_cast<uint32_t>(bindlessHandle >> 32);
        UIVertex2 vertex;
        vertex.Position   = Math::Vec3(x, y, z);
        vertex.TexCoord   = Math::Vec2(u, v);
        vertex.Color      = color;
        vertex.texHandleLo = handleLo;
        vertex.texHandleHi = handleHi;
        return vertex;
    }

    // =========================================================================
    // CalculateFitFontSize
    // =========================================================================

    float UITextMeshGenerator::CalculateFitFontSize(
        const std::string& text,
        const FontAtlas& fontAtlas,
        float maxWidth,
        float maxHeight,
        float baseFontSize,
        float minFontSize,
        float maxFontSize,
        bool wordWrap)
    {
        if (text.empty() || maxWidth <= 0.0f || maxHeight <= 0.0f)
            return baseFontSize;

        float atlasSize = fontAtlas.GetFontSize();
        if (atlasSize <= 0.0f) return baseFontSize;

        float low     = minFontSize;
        float high    = maxFontSize;
        float bestFit = std::clamp(baseFontSize, minFontSize, maxFontSize);

        const int   maxIter   = 10;
        const float tolerance = 0.5f;

        for (int iter = 0; iter < maxIter; ++iter) {
            float testSize   = (low + high) * 0.5f;
            float sf         = testSize / atlasSize;

            std::vector<LineInfo> lines = CalculateLines(text, fontAtlas, maxWidth / sf, wordWrap);
            if (lines.empty()) { high = testSize; continue; }

            float textHeight = lines.size() * fontAtlas.GetLineHeight() * sf;
            float textWidth  = 0.0f;
            for (const auto& li : lines)
                textWidth = std::max(textWidth, li.width * sf);

            if (textWidth <= maxWidth + tolerance && textHeight <= maxHeight + tolerance) {
                bestFit = testSize;
                low     = testSize;
                if (high - low < tolerance) break;
            } else {
                high = testSize;
            }
        }

        return bestFit;
    }

} // namespace NE::Graphics
