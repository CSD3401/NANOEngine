#ifndef NANOENGINE_GRAPHICS_FONT_HPP
#define NANOENGINE_GRAPHICS_FONT_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "ResourceManagement/IResource.hpp"
#include "../OpenGL/GLTexture.hpp"
#include "../../NANOEngineAPI.hpp"

namespace NE::Graphics {

    // Glyph metrics for a single character
    struct GlyphMetrics {
        float width = 0.0f;          // Width of the glyph in pixels
        float height = 0.0f;         // Height of the glyph in pixels
        float advanceX = 0.0f;       // Horizontal advance (how far to move cursor)
        float advanceY = 0.0f;       // Vertical advance
        float bearingX = 0.0f;       // X offset from cursor position
        float bearingY = 0.0f;       // Y offset from cursor position
        float u0 = 0.0f;             // UV coordinate start (U)
        float v0 = 0.0f;             // UV coordinate start (V)
        float u1 = 0.0f;             // UV coordinate end (U)
        float v1 = 0.0f;             // UV coordinate end (V)
    };

    class NANOENGINE_API Font : public Resource::IResource {
    public:
        Font();
        ~Font() override;

        // IResource interface
        bool Preload(Resource::BinaryView blob) override;
        void Finalize() override;

        // Direct file loading (for TTF/OTF files)
        bool LoadFromFile(const std::string& filePath, float fontSize = 16.0f);

        // Get glyph metrics for a character
        const GlyphMetrics* GetGlyphMetrics(uint32_t codepoint) const;
        
        // Get font metrics
        float GetLineHeight() const { return m_lineHeight; }
        float GetAscender() const { return m_ascender; }
        float GetDescender() const { return m_descender; }
        float GetFontSize() const { return m_fontSize; }

        // Get the font atlas texture
        std::shared_ptr<OpenGL::GLTexture> GetAtlasTexture() const { return m_atlasTexture; }
        uint64_t GetAtlasTextureHandle() const;

        // Measure text dimensions
        float MeasureTextWidth(const std::string& text) const;
        float MeasureTextHeight(const std::string& text, float maxWidth = 0.0f) const;

        // Get kerning between two characters
        float GetKerning(uint32_t codepoint1, uint32_t codepoint2) const;

        std::string uuid; // for resource management

    private:
        // Font data
        std::vector<uint8_t> m_fontData;
        float m_fontSize = 16.0f;
        float m_scale = 1.0f;
        
        // Font metrics
        float m_lineHeight = 0.0f;
        float m_ascender = 0.0f;
        float m_descender = 0.0f;
        float m_baseline = 0.0f;

        // Atlas data
        uint32_t m_atlasWidth = 0;
        uint32_t m_atlasHeight = 0;
        std::vector<uint8_t> m_atlasData; // RGBA8 data

        // Glyph storage
        std::unordered_map<uint32_t, GlyphMetrics> m_glyphs;
        std::unordered_map<uint64_t, float> m_kerningPairs; // key = (codepoint1 << 32) | codepoint2

        // Texture
        std::shared_ptr<OpenGL::GLTexture> m_atlasTexture;
        unsigned int m_atlasTextureID = 0; // Raw OpenGL texture ID
        uint64_t m_atlasTextureHandle = 0; // Bindless handle

        // Internal helpers
        bool GenerateAtlas();
        void CalculateKerning();
    };

} // namespace NE::Graphics

#endif // !NANOENGINE_GRAPHICS_FONT_HPP

