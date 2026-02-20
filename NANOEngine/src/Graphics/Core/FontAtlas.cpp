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

        // Determine atlas size (use larger size for SDF quality: 1024x1024)
        // All buckets merged into one 64pt atlas, scaled at render time
        m_atlasWidth = 1024;
        m_atlasHeight = 1024;

        // Allocate atlas bitmap for SDF (GL_R8)
        std::vector<unsigned char> atlasBitmap(m_atlasWidth * m_atlasHeight, 0);

        // Pack glyphs into atlas
        int padding = 4;  // SDF requires padding for falloff (was 1 for bitmap)
        int packX = padding;
        int packY = padding;
        int maxRowHeight = 0;

        for (int c = FIRST_CHAR; c <= LAST_CHAR; ++c) {
            int glyphIndex = stbtt_FindGlyphIndex(&fontInfo, c);

            // Get glyph metrics
            int advanceWidth, leftSideBearing;
            stbtt_GetGlyphHMetrics(&fontInfo, glyphIndex, &advanceWidth, &leftSideBearing);

            // For SDF: use SDF-specific sizing instead of bitmap box
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            stbtt_GetGlyphBitmapBox(&fontInfo, glyphIndex, scale, scale, &x0, &y0, &x1, &y1);
            int glyphWidth = std::max(0, x1 - x0);
            int glyphHeight = std::max(0, y1 - y0);

            // Add padding for SDF falloff
            int sdfWidth = glyphWidth + padding * 2;
            int sdfHeight = glyphHeight + padding * 2;

            // Declare offset variables for SDF (used after glyph rendering)
            int xOffset = 0, yOffset = 0;

            // Check if glyph fits in current row
            if (packX + sdfWidth + padding > m_atlasWidth) {
                // Move to next row
                packX = padding;
                packY += maxRowHeight + padding;
                maxRowHeight = 0;
            }

            // Check if atlas needs to be resized
            if (packY + sdfHeight + padding > m_atlasHeight) {
                // Double atlas height and reallocate
                m_atlasHeight *= 2;
                atlasBitmap.resize(m_atlasWidth * m_atlasHeight, 0);
            }

            // Render glyph to atlas using SDF
            if (glyphWidth > 0 && glyphHeight > 0) {
                unsigned char* sdfBitmap = stbtt_GetCodepointSDF(
                    &fontInfo,
                    scale,
                    c,
                    padding,
                    180,      // onEdgeValue: SDF value at glyph edge (0-255)
                    32.0f,    // pixelDistScale: falloff rate
                    &sdfWidth,
                    &sdfHeight,
                    &xOffset,
                    &yOffset
                );

                if (sdfBitmap) {
                    // Copy SDF bitmap into atlas
                    for (int y = 0; y < sdfHeight; ++y) {
                        for (int x = 0; x < sdfWidth; ++x) {
                            int atlasIdx = (packY + y) * m_atlasWidth + (packX + x);
                            int sdfIdx = y * sdfWidth + x;
                            if (atlasIdx < (int)atlasBitmap.size()) {
                                atlasBitmap[atlasIdx] = sdfBitmap[sdfIdx];
                            }
                        }
                    }
                    stbtt_FreeSDF(sdfBitmap, nullptr);
                }
            }

            // Store glyph info
            GlyphInfo info;
            info.px = packX;
            info.py = packY;

            // Use SDF dimensions (includes padding on all sides) so UVs cover the full
            // SDF falloff region, not just the bitmap-box body. Without this the shader
            // samples only the hard glyph body and never sees the smooth distance gradient
            // in the border, which produces aliased edges instead of smooth SDF rendering.
            info.width = static_cast<float>(sdfWidth);
            info.height = static_cast<float>(sdfHeight);
            // Use the bearing offsets returned by stbtt_GetCodepointSDF so that glyph
            // placement accounts for the SDF padding offset, not the bitmap-box origin.
            info.xOffset = static_cast<float>(xOffset);
            info.yOffset = static_cast<float>(yOffset);
            info.xAdvance = advanceWidth * scale;

            m_glyphs[static_cast<char>(c)] = info;

            // Advance pack position
            packX += sdfWidth + padding;
            maxRowHeight = std::max(maxRowHeight, sdfHeight);
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
        // Three buckets: 32pt (body text 8-32pt), 64pt (mid 32-96pt), 128pt (headers/large)
        // Consolidating from 5 buckets reduces memory: 5 fonts × 3 buckets = 9MB vs 15MB
        static const float buckets[] = { 32.0f, 64.0f, 128.0f };
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
