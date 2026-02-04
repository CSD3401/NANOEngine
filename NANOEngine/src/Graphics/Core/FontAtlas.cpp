#include "FontAtlas.hpp"
#include "Font.hpp"
#include "ResourceManagement/ResourceManager.hpp"
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

    bool FontAtlas::Load(const std::vector<uint8_t>& fontData, float fontSize) {
        // Initialize stb_truetype with provided font data
        stbtt_fontinfo fontInfo;
        if (!stbtt_InitFont(&fontInfo, fontData.data(), 0)) {
            std::cerr << "[FontAtlas] Failed to initialize font" << std::endl;
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

    float FontAtlas::QuantizeToFontBucket(float fontSize) {
        // Font size buckets for memory optimization
        // Requested sizes snap to nearest bucket, then scale at render time
        static const float buckets[] = { 16.0f, 32.0f, 64.0f, 128.0f, 256.0f };
        static const int bucketCount = sizeof(buckets) / sizeof(float);

        // Clamp to valid range
        if (fontSize <= buckets[0]) return buckets[0];
        if (fontSize >= buckets[bucketCount - 1]) return buckets[bucketCount - 1];

        // Find closest bucket (prefer rounding up for better quality)
        for (int i = 0; i < bucketCount; ++i) {
            if (fontSize <= buckets[i]) {
                return buckets[i];
            }
        }

        return buckets[bucketCount - 1];
    }

    std::string FontAtlas::MakeCacheKey(const std::string& fontUUID, float fontSize) {
        float bucketSize = QuantizeToFontBucket(fontSize);
        std::ostringstream oss;
        oss << fontUUID << "@" << static_cast<int>(bucketSize * 10);
        return oss.str();
    }

    // FontAtlasCache implementation
    FontAtlasCache& FontAtlasCache::GetInstance() {
        static FontAtlasCache instance;
        return instance;
    }

    std::shared_ptr<FontAtlas> FontAtlasCache::GetOrCreate(
        const std::string& fontUUID,
        float fontSize
    ) {
        // Check if UUID is empty
        if (fontUUID.empty()) {
            std::cerr << "[FontAtlasCache] Empty font UUID provided" << std::endl;
            return nullptr;
        }

        // Check cache first
        std::string key = FontAtlas::MakeCacheKey(fontUUID, fontSize);
        auto it = m_cache.find(key);
        if (it != m_cache.end()) {
            return it->second;
        }

        // Load Font resource via ResourceManager
        auto& rm = NE::Resource::ResourceManager::GetInstance();
        auto fontResource = rm.LoadResource<Font>(fontUUID);
        if (!fontResource) {
            std::cerr << "[FontAtlasCache] Failed to load Font resource: " << fontUUID << std::endl;
            return nullptr;
        }

        // Create atlas from font data using bucket size (not exact requested size)
        // This allows us to share atlases across similar sizes (e.g., 15pt, 16pt, 17pt all use 16pt bucket)
        float bucketSize = FontAtlas::QuantizeToFontBucket(fontSize);
        auto atlas = std::make_shared<FontAtlas>();
        if (!atlas->Load(fontResource->GetFontData(), bucketSize)) {
            return nullptr;
        }

        m_cache[key] = atlas;
        return atlas;
    }

    void FontAtlasCache::Clear() {
        m_cache.clear();
    }

} // namespace NE::Graphics
