#include "Font.hpp"
#include "Core/SpdLogger.hpp"
#include <glad/glad.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Include stb_truetype
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma warning(push)
#pragma warning(disable: 4244 4242 4456 4701 4702 4996)
#include "../../../extern/jolt/TestFramework/External/stb_truetype.h"
#pragma warning(pop)

namespace NE::Graphics {

    Font::Font() = default;

    Font::~Font() {
        // Clean up OpenGL texture
        if (m_atlasTextureHandle != 0) {
            glMakeTextureHandleNonResidentARB(m_atlasTextureHandle);
        }
        if (m_atlasTextureID != 0) {
            glDeleteTextures(1, &m_atlasTextureID);
        }
    }

    bool Font::Preload(Resource::BinaryView blob) {
        // For now, treat binary data as raw TTF/OTF font data
        // In the future, this could be a custom binary format
        if (blob.size == 0 || !blob.data) {
            return false;
        }

        m_fontData.resize(blob.size);
        std::memcpy(m_fontData.data(), blob.data, blob.size);

        // Default font size if not set
        if (m_fontSize <= 0.0f) {
            m_fontSize = 16.0f;
        }

        return true;
    }

    void Font::Finalize() {
        if (m_fontData.empty()) {
            SPD_WARNING("Font::Finalize: No font data loaded");
            return;
        }

        if (!GenerateAtlas()) {
            SPD_WARNING("Font::Finalize: Failed to generate font atlas");
            return;
        }

        // Create texture from atlas data
        if (m_atlasData.empty() || m_atlasWidth == 0 || m_atlasHeight == 0) {
            SPD_WARNING("Font::Finalize: Invalid atlas data");
            return;
        }

        // Create GL texture manually (since we have raw RGBA data)
        glGenTextures(1, &m_atlasTextureID);
        glBindTexture(GL_TEXTURE_2D, m_atlasTextureID);

        // Upload texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 
            static_cast<GLsizei>(m_atlasWidth), 
            static_cast<GLsizei>(m_atlasHeight), 
            0, GL_RGBA, GL_UNSIGNED_BYTE, m_atlasData.data());

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create bindless handle
        m_atlasTextureHandle = glGetTextureHandleARB(m_atlasTextureID);
        glMakeTextureHandleResidentARB(m_atlasTextureHandle);

        glBindTexture(GL_TEXTURE_2D, 0);

        SPD_INFO("Font atlas generated: " << m_atlasWidth << "x" << m_atlasHeight << " with " << m_glyphs.size() << " glyphs");
    }

    bool Font::LoadFromFile(const std::string& filePath, float fontSize) {
        m_fontSize = fontSize;

        // Read font file
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            SPD_WARNING("Font::LoadFromFile: Failed to open file: " << filePath);
            return false;
        }

        std::streamsize size = file.tellg();
        if (size <= 0) {
            SPD_WARNING("Font::LoadFromFile: Invalid file size: " << filePath);
            return false;
        }

        file.seekg(0, std::ios::beg);
        m_fontData.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(m_fontData.data()), size)) {
            SPD_WARNING("Font::LoadFromFile: Failed to read file: " << filePath);
            return false;
        }

        file.close();

        // Generate atlas
        if (!GenerateAtlas()) {
            SPD_WARNING("Font::LoadFromFile: Failed to generate atlas");
            return false;
        }

        // Create texture (same as Finalize)
        if (m_atlasData.empty() || m_atlasWidth == 0 || m_atlasHeight == 0) {
            SPD_WARNING("Font::LoadFromFile: Invalid atlas data");
            return false;
        }

        glGenTextures(1, &m_atlasTextureID);
        glBindTexture(GL_TEXTURE_2D, m_atlasTextureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
            static_cast<GLsizei>(m_atlasWidth),
            static_cast<GLsizei>(m_atlasHeight),
            0, GL_RGBA, GL_UNSIGNED_BYTE, m_atlasData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        // Create bindless handle
        m_atlasTextureHandle = glGetTextureHandleARB(m_atlasTextureID);
        glMakeTextureHandleResidentARB(m_atlasTextureHandle);

        glBindTexture(GL_TEXTURE_2D, 0);

        SPD_INFO("Font loaded from file: " << filePath << " (size: " << fontSize << ")");
        return true;
    }

    bool Font::GenerateAtlas() {
        if (m_fontData.empty()) {
            return false;
        }

        // Initialize stb_truetype font
        stbtt_fontinfo font;
        if (!stbtt_InitFont(&font, m_fontData.data(), stbtt_GetFontOffsetForIndex(m_fontData.data(), 0))) {
            SPD_WARNING("Font::GenerateAtlas: Failed to initialize font");
            return false;
        }

        // Calculate scale for desired font size
        m_scale = stbtt_ScaleForPixelHeight(&font, m_fontSize);

        // Get font metrics
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
        m_ascender = ascent * m_scale;
        m_descender = descent * m_scale;
        m_lineHeight = (ascent - descent + lineGap) * m_scale;
        m_baseline = m_ascender;

        // Character range: ASCII printable characters (32-126) plus extended ASCII (128-255)
        constexpr int beginChar = 32;
        constexpr int endChar = 256;

        // Start with reasonable atlas size
        m_atlasWidth = 512;
        m_atlasHeight = 512;

        // Try to pack all glyphs - calculate required size
        int padding = 2;
        int currentX = padding;
        int currentY = padding;
        int rowHeight = 0;
        int maxWidth = 0;

        // First pass: calculate required size
        bool needsResize = true;
        while (needsResize) {
            needsResize = false;
            currentX = padding;
            currentY = padding;
            rowHeight = 0;

            for (int c = beginChar; c < endChar; ++c) {
                int w, h, xoff, yoff;
                unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, m_scale, c, &w, &h, &xoff, &yoff);
                if (bitmap) {
                    STBTT_free(bitmap, nullptr);
                    rowHeight = std::max(rowHeight, h + padding * 2);

                    if (currentX + w + padding * 2 > static_cast<int>(m_atlasWidth)) {
                        currentX = padding;
                        currentY += rowHeight;
                        rowHeight = h + padding * 2;
                    }

                    if (currentY + h + padding * 2 > static_cast<int>(m_atlasHeight)) {
                        // Need to expand atlas
                        m_atlasHeight *= 2;
                        needsResize = true;
                        break;
                    }

                    currentX += w + padding * 2;
                    maxWidth = std::max(maxWidth, currentX);
                }
            }
        }

        // Round up to power of 2 for better GPU performance
        m_atlasWidth = 1;
        while (m_atlasWidth < maxWidth) m_atlasWidth <<= 1;
        if (m_atlasHeight < currentY + rowHeight) {
            m_atlasHeight = 1;
            while (m_atlasHeight < currentY + rowHeight) m_atlasHeight <<= 1;
        }

        // Allocate atlas data (RGBA8)
        m_atlasData.resize(m_atlasWidth * m_atlasHeight * 4, 0);

        // Second pass: render glyphs
        currentX = padding;
        currentY = padding;
        rowHeight = 0;

        for (int c = beginChar; c < endChar; ++c) {
            int w, h, xoff, yoff;
            unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, m_scale, c, &w, &h, &xoff, &yoff);
            if (!bitmap) continue;

            // Check if glyph fits on current line
            if (currentX + w + padding * 2 > static_cast<int>(m_atlasWidth)) {
                currentX = padding;
                currentY += rowHeight;
                rowHeight = 0;
            }

            rowHeight = std::max(rowHeight, h + padding * 2);

            // Get glyph metrics
            int advanceWidth, leftSideBearing;
            stbtt_GetCodepointHMetrics(&font, c, &advanceWidth, &leftSideBearing);

            // yoff is the offset from the top of the bitmap to the baseline
            // bearingY should be the distance from baseline to top of character (positive = above baseline)
            // In stb_truetype: yoff is typically negative (baseline is below top of bitmap)
            // So bearingY = -yoff gives us the distance from baseline to top
            float bearingY = -yoff * m_scale;

            GlyphMetrics metrics;
            metrics.width = static_cast<float>(w);
            metrics.height = static_cast<float>(h);
            metrics.advanceX = advanceWidth * m_scale;
            metrics.bearingX = leftSideBearing * m_scale;
            metrics.bearingY = bearingY;
            metrics.u0 = static_cast<float>(currentX) / static_cast<float>(m_atlasWidth);
            metrics.v0 = static_cast<float>(currentY) / static_cast<float>(m_atlasHeight);
            metrics.u1 = static_cast<float>(currentX + w) / static_cast<float>(m_atlasWidth);
            metrics.v1 = static_cast<float>(currentY + h) / static_cast<float>(m_atlasHeight);

            // Copy bitmap to atlas (convert grayscale to RGBA)
            for (int j = 0; j < h; ++j) {
                for (int i = 0; i < w; ++i) {
                    int atlasX = currentX + i;
                    int atlasY = currentY + j;
                    if (atlasX >= 0 && atlasX < static_cast<int>(m_atlasWidth) &&
                        atlasY >= 0 && atlasY < static_cast<int>(m_atlasHeight)) {
                        size_t atlasIdx = (atlasY * m_atlasWidth + atlasX) * 4;
                        unsigned char alpha = bitmap[j * w + i];
                        m_atlasData[atlasIdx + 0] = 255; // R
                        m_atlasData[atlasIdx + 1] = 255; // G
                        m_atlasData[atlasIdx + 2] = 255; // B
                        m_atlasData[atlasIdx + 3] = alpha; // A
                    }
                }
            }

            m_glyphs[static_cast<uint32_t>(c)] = metrics;

            currentX += w + padding * 2;

            STBTT_free(bitmap, nullptr);
        }

        // Calculate kerning
        CalculateKerning();

        return true;
    }


    void Font::CalculateKerning() {
        if (m_fontData.empty()) return;

        stbtt_fontinfo font;
        if (!stbtt_InitFont(&font, m_fontData.data(), stbtt_GetFontOffsetForIndex(m_fontData.data(), 0))) {
            return;
        }

        // Calculate kerning for all glyph pairs
        for (const auto& [cp1, _] : m_glyphs) {
            for (const auto& [cp2, __] : m_glyphs) {
                int kern = stbtt_GetCodepointKernAdvance(&font, static_cast<int>(cp1), static_cast<int>(cp2));
                if (kern != 0) {
                    uint64_t key = (static_cast<uint64_t>(cp1) << 32) | static_cast<uint64_t>(cp2);
                    m_kerningPairs[key] = kern * m_scale;
                }
            }
        }
    }

    const GlyphMetrics* Font::GetGlyphMetrics(uint32_t codepoint) const {
        auto it = m_glyphs.find(codepoint);
        if (it != m_glyphs.end()) {
            return &it->second;
        }
        return nullptr;
    }

    uint64_t Font::GetAtlasTextureHandle() const {
        return m_atlasTextureHandle;
    }

    float Font::MeasureTextWidth(const std::string& text) const {
        float width = 0.0f;
        float maxWidth = 0.0f;

        for (size_t i = 0; i < text.length(); ++i) {
            uint32_t codepoint = static_cast<unsigned char>(text[i]);

            if (codepoint == '\n') {
                maxWidth = std::max(maxWidth, width);
                width = 0.0f;
                continue;
            }

            const GlyphMetrics* metrics = GetGlyphMetrics(codepoint);
            if (metrics) {
                width += metrics->advanceX;

                // Add kerning if next character exists
                if (i + 1 < text.length()) {
                    uint32_t nextCodepoint = static_cast<unsigned char>(text[i + 1]);
                    width += GetKerning(codepoint, nextCodepoint);
                }
            }
        }

        return std::max(maxWidth, width);
    }

    float Font::MeasureTextHeight(const std::string& text, float maxWidth) const {
        if (text.empty()) return 0.0f;

        float height = m_lineHeight;
        float currentWidth = 0.0f;

        for (size_t i = 0; i < text.length(); ++i) {
            uint32_t codepoint = static_cast<unsigned char>(text[i]);

            if (codepoint == '\n') {
                height += m_lineHeight;
                currentWidth = 0.0f;
                continue;
            }

            const GlyphMetrics* metrics = GetGlyphMetrics(codepoint);
            if (metrics) {
                currentWidth += metrics->advanceX;

                if (i + 1 < text.length()) {
                    uint32_t nextCodepoint = static_cast<unsigned char>(text[i + 1]);
                    currentWidth += GetKerning(codepoint, nextCodepoint);
                }

                if (maxWidth > 0.0f && currentWidth > maxWidth) {
                    height += m_lineHeight;
                    currentWidth = metrics->advanceX;
                }
            }
        }

        return height;
    }

    float Font::GetKerning(uint32_t codepoint1, uint32_t codepoint2) const {
        uint64_t key = (static_cast<uint64_t>(codepoint1) << 32) | static_cast<uint64_t>(codepoint2);
        auto it = m_kerningPairs.find(key);
        if (it != m_kerningPairs.end()) {
            return it->second;
        }
        return 0.0f;
    }

} // namespace NE::Graphics

