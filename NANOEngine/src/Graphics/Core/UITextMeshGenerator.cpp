#include "UITextMeshGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <iostream>

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
        // Handle horizontal overflow WRAP mode - enable word wrap if overflow is WRAP
        bool shouldWrap = text.wordWrap || (text.horizontalOverflow == NE::ECS::Component::UIText::OverflowMode::WRAP);
        float maxWidth = (shouldWrap && width > 0.0f) ? width : 0.0f;
        std::vector<TextLine> lines = CalculateTextLines(text.text, font, maxWidth, shouldWrap);

        if (lines.empty()) {
            return {};
        }

        // Handle vertical overflow TRUNCATE - limit lines based on height
        float lineHeight = font.GetLineHeight() * text.lineSpacing;
        size_t maxLines = lines.size();
        if (text.verticalOverflow == NE::ECS::Component::UIText::OverflowMode::TRUNCATE && height > 0.0f) {
            maxLines = static_cast<size_t>(std::floor(height / lineHeight));
            if (maxLines < lines.size()) {
                lines.resize(maxLines);
            }
        }

        if (lines.empty()) {
            return {};
        }

        // Calculate actual rendered bounds by checking all glyphs
        // Font-level ascender/descender might not match actual glyph extents
        float maxBearingY = 0.0f;  // Maximum distance above baseline
        float maxGlyphHeight = 0.0f;  // Maximum glyph height
        float maxExtentBelowBaseline = 0.0f;  // Maximum extent below baseline
        
        // Pre-pass: Calculate actual glyph extents for all characters
        for (const auto& line : lines) {
            for (size_t i = 0; i < line.text.length(); ++i) {
                uint32_t codepoint = static_cast<unsigned char>(line.text[i]);
                if (codepoint < 32 && codepoint != '\n') continue;
                
                const GlyphMetrics* metrics = font.GetGlyphMetrics(codepoint);
                if (!metrics) continue;
                
                // bearingY is distance from baseline upward (positive = above baseline)
                maxBearingY = std::max(maxBearingY, metrics->bearingY);
                
                // Glyph height
                maxGlyphHeight = std::max(maxGlyphHeight, metrics->height);
                
                // Extent below baseline = glyphHeight - bearingY
                // This is how far the glyph extends below the baseline
                float extentBelow = metrics->height - metrics->bearingY;
                maxExtentBelowBaseline = std::max(maxExtentBelowBaseline, extentBelow);
            }
        }
        
        // Use actual glyph extents instead of font-level metrics
        // This ensures we account for glyphs that extend beyond font metrics
        float actualAscender = maxBearingY > 0.0f ? maxBearingY : font.GetAscender();
        float actualDescender = maxExtentBelowBaseline > 0.0f ? maxExtentBelowBaseline : std::abs(font.GetDescender());
        
        // Calculate total visual height using actual glyph extents
        // For single line: actualAscender + actualDescender
        // For multiple lines: actualAscender + (lines-1) * lineHeight + actualDescender
        float totalTextHeight;
        if (lines.size() == 1) {
            totalTextHeight = actualAscender + actualDescender;
        } else {
            totalTextHeight = actualAscender + (static_cast<float>(lines.size() - 1) * lineHeight) + actualDescender;
        }
        
        // Keep font metrics for reference
        float ascender = font.GetAscender();
        float descender = std::abs(font.GetDescender());
        
        // Calculate vertical offset to position text block correctly within container
        // This offset positions the TOP of the text block (baseline - ascender)
        float verticalOffset = 0.0f;
        switch (text.verticalAlign) {
        case NE::ECS::Component::UIText::VerticalAlignment::TOP:
            // Top of text block (baseline - ascender) aligns with container top
            verticalOffset = 0.0f;
            break;
            
        case NE::ECS::Component::UIText::VerticalAlignment::MIDDLE:
            // Center of text block aligns with container center
            verticalOffset = (height - totalTextHeight) * 0.5f;
            break;
            
        case NE::ECS::Component::UIText::VerticalAlignment::BOTTOM:
            // Bottom of text block (baseline + descender) aligns with container bottom
            verticalOffset = height - totalTextHeight;
            break;
        }

        // Container boundaries for clipping
        float containerLeft = x;
        float containerRight = x + width;
        float containerTop = y;
        float containerBottom = y + height;

        // Generate vertices for each line
        // Start from top (y is top-left in top-down coordinate system)
        // For BOTTOM alignment: position so text block bottom aligns with container bottom
        // For TOP alignment: position so text block top aligns with container top
        // For MIDDLE alignment: center the text block
        float firstLineBaseline;
        switch (text.verticalAlign) {
        case NE::ECS::Component::UIText::VerticalAlignment::TOP: {
            // Text block top at container top, baseline is below by actualAscender
            firstLineBaseline = y + actualAscender;
            break;
        }
        case NE::ECS::Component::UIText::VerticalAlignment::MIDDLE: {
            // Center text block: container center - (totalTextHeight / 2) + actualAscender
            firstLineBaseline = y + (height - totalTextHeight) * 0.5f + actualAscender;
            break;
        }
        case NE::ECS::Component::UIText::VerticalAlignment::BOTTOM: {
            // Text block bottom at container bottom
            // Text block top = container bottom - totalTextHeight
            // Baseline = text block top + actualAscender = container bottom - totalTextHeight + actualAscender
            firstLineBaseline = y + height - totalTextHeight + actualAscender;
            break;
        }
        }
        
        float firstLineTop = firstLineBaseline - actualAscender;
        float currentY = firstLineBaseline; // Baseline of first line
        
        // Calculate text block boundaries
        // Text block top is at first line top (baseline - ascender)
        // Text block bottom is at first line top + totalTextHeight
        float textBlockTop = firstLineTop;
        float textBlockBottom = firstLineTop + totalTextHeight;
        float textBlockLeft = x;
        float textBlockRight = x + width; // Will be updated with actual text width if needed
        
        // Calculate actual text width (max line width)
        float actualTextWidth = 0.0f;
        for (const auto& line : lines) {
            if (line.width > actualTextWidth) {
                actualTextWidth = line.width;
            }
        }
        
        // Determine horizontal text block bounds based on alignment
        switch (text.horizontalAlign) {
        case NE::ECS::Component::UIText::Alignment::LEFT:
            textBlockRight = textBlockLeft + actualTextWidth;
            break;
        case NE::ECS::Component::UIText::Alignment::CENTER:
            textBlockLeft = x + (width - actualTextWidth) * 0.5f;
            textBlockRight = textBlockLeft + actualTextWidth;
            break;
        case NE::ECS::Component::UIText::Alignment::RIGHT:
            textBlockLeft = x + width - actualTextWidth;
            textBlockRight = x + width;
            break;
        default:
            textBlockRight = textBlockLeft + actualTextWidth;
            break;
        }
        

        for (const auto& line : lines) {
            // Check vertical overflow TRUNCATE - skip lines that exceed container height
            if (text.verticalOverflow == NE::ECS::Component::UIText::OverflowMode::TRUNCATE && height > 0.0f) {
                // currentY is the baseline, so line top is baseline - actualAscender
                float lineTop = currentY - actualAscender;
                float lineBottom = currentY + actualDescender; // Bottom of line is baseline + actualDescender
                
                // Skip if line is completely outside container
                if (lineBottom > containerBottom || lineTop < containerTop) {
                    currentY += lineHeight;
                    continue;
                }
            }

            // Calculate horizontal alignment offset
            float horizontalOffset = CalculateHorizontalOffset(line, font, width, text.horizontalAlign);

            float currentX = x + horizontalOffset;

            // Track actual rendered bounds for this line
            float lineActualTop = currentY;
            float lineActualBottom = currentY;

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

                // Calculate character position
                float charX = currentX + metrics->bearingX;
                float charRight = charX + metrics->width;
                
                // Calculate actual character bounds (charY is top of character quad)
                // Fix baseline alignment: ensure all characters sit on the same baseline
                // The baseline is where characters "sit" - all should align here
                // Position character so its baseline aligns with text baseline (currentY)
                // The glyph's baseline is 'metrics->bearingY' pixels below its top
                // So: charY + metrics->bearingY = currentY (baseline)
                // Therefore: charY = currentY - metrics->bearingY
                // This ensures all characters have their baseline at the same Y position
                float charY = currentY - metrics->bearingY;
                float charTop = charY;
                float charBottom = charY + metrics->height;
                
                // Update line bounds
                if (i == 0 || charTop < lineActualTop) {
                    lineActualTop = charTop;
                }
                if (i == 0 || charBottom > lineActualBottom) {
                    lineActualBottom = charBottom;
                }

                // Handle horizontal overflow TRUNCATE - skip characters that exceed width
                if (text.horizontalOverflow == NE::ECS::Component::UIText::OverflowMode::TRUNCATE && width > 0.0f) {
                    // Skip if character is completely outside container
                    if (charX >= containerRight) {
                        break; // Stop rendering this line
                    }
                    
                    // Clip character if it partially exceeds bounds
                    if (charRight > containerRight) {
                        // Character exceeds bounds - we could clip it, but for simplicity, just stop
                        break;
                    }
                }

                // Generate quad for this character (with clipping if needed)
                // Pass actualAscender to normalize character alignment
                if (text.horizontalOverflow == NE::ECS::Component::UIText::OverflowMode::TRUNCATE && width > 0.0f) {
                    GenerateCharacterQuadClipped(vertices, *metrics, currentX, currentY, z, color, containerLeft, containerRight, actualAscender);
                } else {
                    // VISIBLE mode - render fully
                    GenerateCharacterQuad(vertices, *metrics, currentX, currentY, z, color, actualAscender);
                }

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
            currentY += lineHeight;
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
        const Math::Vec4& color,
        float normalizedAscender
    ) {
        // Calculate character position
        // bearingX: horizontal offset from cursor (can be negative for some characters)
        // For Y: position character so its baseline aligns with text baseline
        // The baseline is where characters "sit" - all should align here
        // The glyph's baseline is 'metrics.bearingY' pixels below its top
        // So: charY + metrics.bearingY = y (baseline)
        // Therefore: charY = y - metrics.bearingY
        // This ensures all characters have their baseline at the same Y position
        float charX = x + metrics.bearingX;
        float charY = y - metrics.bearingY;

        // Character quad dimensions
        float charWidth = metrics.width;
        float charHeight = metrics.height;

        // Generate quad (2 triangles, 6 vertices)
        // Top-left origin system (Y-down)
        // UV coordinates from font atlas (flip V coordinates to fix upside-down text)
        vertices.push_back(CreateVertex(
            charX, charY, z,
            metrics.u0, metrics.v0, // Top-left of glyph in atlas
            color
        ));

        vertices.push_back(CreateVertex(
            charX + charWidth, charY, z,
            metrics.u1, metrics.v0, // Top-right of glyph in atlas
            color
        ));

        vertices.push_back(CreateVertex(
            charX + charWidth, charY + charHeight, z,
            metrics.u1, metrics.v1, // Bottom-right of glyph in atlas
            color
        ));

        // Second triangle
        vertices.push_back(CreateVertex(
            charX, charY, z,
            metrics.u0, metrics.v0, // Top-left
            color
        ));

        vertices.push_back(CreateVertex(
            charX + charWidth, charY + charHeight, z,
            metrics.u1, metrics.v1, // Bottom-right
            color
        ));

        vertices.push_back(CreateVertex(
            charX, charY + charHeight, z,
            metrics.u0, metrics.v1, // Bottom-left of glyph in atlas
            color
        ));
    }

    void UITextMeshGenerator::GenerateCharacterQuadClipped(
        std::vector<UIVertex>& vertices,
        const GlyphMetrics& metrics,
        float x, float y, float z,
        const Math::Vec4& color,
        float clipLeft,
        float clipRight,
        float normalizedAscender
    ) {
        // Calculate character position
        // Use normalized ascender to ensure consistent baseline alignment
        float charX = x + metrics.bearingX;
        // All characters use the same top offset (normalizedAscender) from baseline
        float charY = y - normalizedAscender;
        // Adjust for glyph's actual baseline position
        float baselineAdjustment = normalizedAscender - metrics.bearingY;
        charY += baselineAdjustment;
        float charWidth = metrics.width;
        float charHeight = metrics.height;
        float charRight = charX + charWidth;

        // Clip horizontally if needed
        float clippedLeft = std::max(charX, clipLeft);
        float clippedRight = std::min(charRight, clipRight);
        float clippedWidth = clippedRight - clippedLeft;

        // If character is completely outside clip bounds, don't render
        if (clippedWidth <= 0.0f || clippedLeft >= clipRight) {
            return;
        }

        // Calculate UV clipping (proportional to position clipping)
        float uClipLeft = metrics.u0;
        float uClipRight = metrics.u1;
        if (charWidth > 0.0f) {
            float uRatio = (clippedLeft - charX) / charWidth;
            uClipLeft = metrics.u0 + (metrics.u1 - metrics.u0) * uRatio;
            uRatio = (clippedRight - charX) / charWidth;
            uClipRight = metrics.u0 + (metrics.u1 - metrics.u0) * uRatio;
        }

        // Generate clipped quad (2 triangles, 6 vertices)
        vertices.push_back(CreateVertex(
            clippedLeft, charY, z,
            uClipLeft, metrics.v0, // Top-left (clipped)
            color
        ));

        vertices.push_back(CreateVertex(
            clippedRight, charY, z,
            uClipRight, metrics.v0, // Top-right (clipped)
            color
        ));

        vertices.push_back(CreateVertex(
            clippedRight, charY + charHeight, z,
            uClipRight, metrics.v1, // Bottom-right (clipped)
            color
        ));

        // Second triangle
        vertices.push_back(CreateVertex(
            clippedLeft, charY, z,
            uClipLeft, metrics.v0, // Top-left (clipped)
            color
        ));

        vertices.push_back(CreateVertex(
            clippedRight, charY + charHeight, z,
            uClipRight, metrics.v1, // Bottom-right (clipped)
            color
        ));

        vertices.push_back(CreateVertex(
            clippedLeft, charY + charHeight, z,
            uClipLeft, metrics.v1, // Bottom-left (clipped)
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
        NE::ECS::Component::UIText::VerticalAlignment alignment,
        float lineSpacing
    ) {
        if (containerHeight <= 0.0f || lines.empty()) {
            return 0.0f;
        }

        // Calculate total text height from baseline to baseline
        // Note: lineHeight already includes ascender + descender + lineGap
        float totalTextHeight = static_cast<float>(lines.size()) * font.GetLineHeight() * lineSpacing;

        switch (alignment) {
        case NE::ECS::Component::UIText::VerticalAlignment::TOP:
            // Top alignment: position top of text at container top
            // The baseline is below the top by the ascender amount
            // So we return 0, and baselineOffset will be added later
            return 0.0f;

        case NE::ECS::Component::UIText::VerticalAlignment::MIDDLE:
            // Middle alignment: center the text block vertically
            // The offset positions the top of the text block
            return (containerHeight - totalTextHeight) * 0.5f;

        case NE::ECS::Component::UIText::VerticalAlignment::BOTTOM:
            // Bottom alignment: position bottom of text at container bottom
            // The offset positions the top of the text block
            return containerHeight - totalTextHeight;

        default:
            return 0.0f;
        }
    }

} // namespace NE::Graphics

