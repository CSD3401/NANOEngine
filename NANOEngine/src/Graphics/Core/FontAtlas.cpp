#include "FontAtlas.hpp"
#include <glad/glad.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>

// Define stb_truetype implementation in this compilation unit
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../../extern/jolt/TestFramework/External/stb_truetype.h"

namespace NE::Graphics {

    FontAtlas::~FontAtlas() {
        Unload();
    }

    FontAtlas::FontAtlas(FontAtlas&& other) noexcept
        : m_glyphs(std::move(other.m_glyphs))
        , m_textureID(other.m_textureID)
        , m_bindlessHandle(other.m_bindlessHandle)
        , m_fontSize(other.m_fontSize)
        , m_lineHeight(other.m_lineHeight)
        , m_ascent(other.m_ascent)
        , m_descent(other.m_descent)
        , m_atlasWidth(other.m_atlasWidth)
        , m_atlasHeight(other.m_atlasHeight)
    {
        other.m_textureID = 0;
        other.m_bindlessHandle = 0;
    }

    FontAtlas& FontAtlas::operator=(FontAtlas&& other) noexcept {
        if (this != &other) {
            Unload();
            m_glyphs = std::move(other.m_glyphs);
            m_textureID = other.m_textureID;
            m_bindlessHandle = other.m_bindlessHandle;
            m_fontSize = other.m_fontSize;
            m_lineHeight = other.m_lineHeight;
            m_ascent = other.m_ascent;
            m_descent = other.m_descent;
            m_atlasWidth = other.m_atlasWidth;
            m_atlasHeight = other.m_atlasHeight;

            other.m_textureID = 0;
            other.m_bindlessHandle = 0;
        }
        return *this;
    }

    bool FontAtlas::Load(const std::filesystem::path& fontPath, float fontSize) {
        // Read font file
        std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::cerr << "[FontAtlas] Failed to open font file: " << fontPath << std::endl;
            return false;
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> fontBuffer(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(fontBuffer.data()), size)) {
            std::cerr << "[FontAtlas] Failed to read font file: " << fontPath << std::endl;
            return false;
        }

        // Initialize stb_truetype
        stbtt_fontinfo fontInfo;
        if (!stbtt_InitFont(&fontInfo, fontBuffer.data(), 0)) {
            std::cerr << "[FontAtlas] Failed to initialize font: " << fontPath << std::endl;
            return false;
        }

        m_fontSize = fontSize;

        // Calculate scale
        float scale = stbtt_ScaleForPixelHeight(&fontInfo, fontSize);

        // Get font metrics
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
        m_ascent = ascent * scale;
        m_descent = descent * scale;
        m_lineHeight = (ascent - descent + lineGap) * scale;

        // Determine atlas size (start with a reasonable size and expand if needed)
        m_atlasWidth = 512;
        m_atlasHeight = 512;

        // Allocate atlas bitmap
        std::vector<unsigned char> atlasBitmap(m_atlasWidth * m_atlasHeight, 0);

        // Pack glyphs into atlas
        int packX = 1; // Start with 1 pixel padding
        int packY = 1;
        int maxRowHeight = 0;

        for (int c = FIRST_CHAR; c <= LAST_CHAR; ++c) {
            int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, c);

            // Get glyph metrics
            int advanceWidth, leftSideBearing;
            stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceWidth, &leftSideBearing);

            int x0, y0, x1, y1;
            stbtt_GetGlyphBitmapBox(&fontInfo, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);

            int glyphWidth = x1 - x0;
            int glyphHeight = y1 - y0;

            // Check if glyph fits in current row
            if (packX + glyphWidth + 1 > m_atlasWidth) {
                // Move to next row
                packX = 1;
                packY += maxRowHeight + 1;
                maxRowHeight = 0;
            }

            // Check if atlas needs to be resized
            if (packY + glyphHeight + 1 > m_atlasHeight) {
                // Double atlas height and reallocate
                m_atlasHeight *= 2;
                atlasBitmap.resize(m_atlasWidth * m_atlasHeight, 0);
            }

            // Render glyph to atlas
            if (glyphWidth > 0 && glyphHeight > 0) {
                stbtt_MakeGlyphBitmap(
                    &fontInfo,
                    atlasBitmap.data() + packY * m_atlasWidth + packX,
                    glyphWidth,
                    glyphHeight,
                    m_atlasWidth,
                    scale,
                    scale,
                    glyphIndex
                );
            }

            // Store glyph info
            GlyphInfo info;
            info.px = packX;
            info.py = packY;

            info.width = static_cast<float>(glyphWidth);
            info.height = static_cast<float>(glyphHeight);
            info.xOffset = static_cast<float>(x0);
            info.yOffset = static_cast<float>(y0);
            info.xAdvance = advanceWidth * scale;

            m_glyphs[static_cast<char>(c)] = info;

            // Advance pack position
            packX += glyphWidth + 1;
            maxRowHeight = std::max(maxRowHeight, glyphHeight);
        }

        for (auto& [ch, g] : m_glyphs) {
            const float x0p = static_cast<float>(g.px);
            const float y0p = static_cast<float>(g.py);
            const float x1p = x0p + g.width;
            const float y1p = y0p + g.height;

            g.u0 = x0p / static_cast<float>(m_atlasWidth);
            g.v0 = y0p / static_cast<float>(m_atlasHeight);
            g.u1 = x1p / static_cast<float>(m_atlasWidth);
            g.v1 = y1p / static_cast<float>(m_atlasHeight);
        }

        // Create OpenGL texture
        glGenTextures(1, &m_textureID);
        glBindTexture(GL_TEXTURE_2D, m_textureID);

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload texture data (single channel - red)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R8,
            m_atlasWidth,
            m_atlasHeight,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            atlasBitmap.data()
        );

        // Create bindless handle
        m_bindlessHandle = glGetTextureHandleARB(m_textureID);
        glMakeTextureHandleResidentARB(m_bindlessHandle);

        glBindTexture(GL_TEXTURE_2D, 0);

        return true;
    }

    void FontAtlas::Unload() {
        if (m_bindlessHandle != 0) {
            glMakeTextureHandleNonResidentARB(m_bindlessHandle);
            m_bindlessHandle = 0;
        }

        if (m_textureID != 0) {
            glDeleteTextures(1, &m_textureID);
            m_textureID = 0;
        }

        m_glyphs.clear();
    }

    const GlyphInfo* FontAtlas::GetGlyph(char c) const {
        auto it = m_glyphs.find(c);
        if (it != m_glyphs.end()) {
            return &it->second;
        }
        // Return space glyph as fallback
        auto fallback = m_glyphs.find(' ');
        return fallback != m_glyphs.end() ? &fallback->second : nullptr;
    }

    std::string FontAtlas::MakeCacheKey(const std::filesystem::path& fontPath, float fontSize) {
        std::ostringstream oss;
        oss << fontPath.string() << "@" << static_cast<int>(fontSize * 10);
        return oss.str();
    }

    // FontAtlasCache implementation
    FontAtlasCache& FontAtlasCache::GetInstance() {
        static FontAtlasCache instance;
        return instance;
    }

    std::shared_ptr<FontAtlas> FontAtlasCache::GetOrCreate(
        const std::filesystem::path& fontPath,
        float fontSize
    ) 
    {
        std::string key = FontAtlas::MakeCacheKey(fontPath, fontSize);

        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return it->second;
        }

        auto atlas = std::make_shared<FontAtlas>();
        if (!atlas->Load(fontPath, fontSize)) {
            return nullptr;
        }

        m_cache[key] = atlas;
        return atlas;
    }

    void FontAtlasCache::Clear() {
        m_cache.clear();
    }

} // namespace NE::Graphics
