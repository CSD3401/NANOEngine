#ifndef FONT_ATLAS_HPP
#define FONT_ATLAS_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <filesystem>
#include <cstdint>

namespace NE::Graphics {

    struct GlyphInfo {
        float u0, v0;           // Top-left UV
        float u1, v1;           // Bottom-right UV
        float width, height;    // Glyph size in pixels
        float xOffset, yOffset; // Offset from cursor position
        float xAdvance;         // How far to advance cursor after this glyph
    };

    class FontAtlas {
    public:
        FontAtlas() = default;
        ~FontAtlas();

        // Non-copyable
        FontAtlas(const FontAtlas&) = delete;
        FontAtlas& operator=(const FontAtlas&) = delete;

        // Moveable
        FontAtlas(FontAtlas&& other) noexcept;
        FontAtlas& operator=(FontAtlas&& other) noexcept;

        bool Load(const std::vector<uint8_t>& fontData, float fontSize);
        void Unload();

        const GlyphInfo* GetGlyph(char c) const;
        uint64_t GetBindlessHandle() const { return m_bindlessHandle; }
        float GetFontSize() const { return m_fontSize; }
        float GetLineHeight() const { return m_lineHeight; }
        float GetAscent() const { return m_ascent; }
        float GetDescent() const { return m_descent; }
        unsigned int GetTextureID() const { return m_textureID; }
        int GetAtlasWidth() const { return m_atlasWidth; }
        int GetAtlasHeight() const { return m_atlasHeight; }

        static std::string MakeCacheKey(const std::string& fontUUID, float fontSize);

    private:
        std::unordered_map<char, GlyphInfo> m_glyphs;
        unsigned int m_textureID = 0;
        uint64_t m_bindlessHandle = 0;
        float m_fontSize = 0.0f;
        float m_lineHeight = 0.0f;
        float m_ascent = 0.0f;
        float m_descent = 0.0f;
        int m_atlasWidth = 0;
        int m_atlasHeight = 0;

        static constexpr int FIRST_CHAR = 32;  // ASCII space
        static constexpr int LAST_CHAR = 126;  // ASCII tilde
        static constexpr int CHAR_COUNT = LAST_CHAR - FIRST_CHAR + 1;
    };

    class FontAtlasCache {
    public:
        static FontAtlasCache& GetInstance();

        std::shared_ptr<FontAtlas> GetOrCreate(const std::string& fontUUID, float fontSize);
        void Clear();

    private:
        FontAtlasCache() = default;
        std::unordered_map<std::string, std::shared_ptr<FontAtlas>> m_cache;
    };

} // namespace NE::Graphics

#endif // FONT_ATLAS_HPP
