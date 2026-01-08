#ifndef TEXT_HPP
#define TEXT_HPP

#include <string>
#include <filesystem>
#include <memory>

#include "../../Math/Vec4.hpp"
#include "../../Core/Reflection.hpp"
#include "../../Graphics/Core/Material.hpp"

namespace NE::ECS::Component {

    struct Text {
        // LUID for serialization
        uint64_t luid = 0;

        // Text Content
        std::string text = "New Text";
        std::string fontUUID;
        std::string fontPath;  // Source file path for the font (used for loading)
        float fontSize = 36.0f;  // Default font size
        NE::Math::Vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };  // White color for better visibility on dark backgrounds

        // Font Style
        enum class FontStyle {
            NORMAL,
            BOLD,
            ITALIC,
            BOLD_AND_ITALIC
        };
        FontStyle fontStyle = FontStyle::NORMAL;

        // Alignment
        enum class Alignment {
            LEFT,
            CENTER,
            RIGHT,
            JUSTIFY
        };
        Alignment horizontalAlign = Alignment::LEFT;

        enum class VerticalAlignment {
            TOP,
            MIDDLE,
            BOTTOM
        };
        VerticalAlignment verticalAlign = VerticalAlignment::MIDDLE;

        // Text Overflow
        enum class OverflowMode {
            WRAP,
            VISIBLE,      // Text is visible and can overflow bounds (renamed from OVERFLOW to avoid Windows macro conflict)
            TRUNCATE
        };
        OverflowMode horizontalOverflow = OverflowMode::VISIBLE;
        OverflowMode verticalOverflow = OverflowMode::TRUNCATE;

        // Best Fit (Auto-sizing)
        bool bestFit = false;
        float minSize = 10.0f;
        float maxSize = 40.0f;

        // Text Spacing
        float lineSpacing = 1.0f;
        float characterSpacing = 0.0f;
        float wordSpacing = 0.0f;
        float paragraphSpacing = 0.0f;

        // Rich Text
        bool richText = false;

        // Material Support
        std::string materialUUID;

        // Raycast Target
        bool raycastTarget = true;

        // Runtime-only fields (not serialized)
        uint64_t fontHandle = 0;
        float textWidth = 0.0f;
        float textHeight = 0.0f;
        std::shared_ptr<NE::Graphics::Material> material;

        // Cached data for auto-size optimization
        mutable std::vector<uint8_t> cachedFontData;
        mutable std::string cachedFontUUID;
        mutable std::string cachedFontPath;
        mutable float cachedContainerWidth = -1.0f;
        mutable float cachedContainerHeight = -1.0f;

        // Reflection - Note: Enum fields are serialized as their underlying integer type
        NE_REFLECT_BEGIN(Text)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(text),
            NE_REFLECT_FIELD(fontUUID),
            NE_REFLECT_FIELD(fontPath),
            NE_REFLECT_FIELD(fontSize),
            NE_REFLECT_FIELD(fontStyle),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD(horizontalAlign),
            NE_REFLECT_FIELD(verticalAlign),
            NE_REFLECT_FIELD(horizontalOverflow),
            NE_REFLECT_FIELD(verticalOverflow),
            NE_REFLECT_FIELD(bestFit),
            NE_REFLECT_FIELD(minSize),
            NE_REFLECT_FIELD(maxSize),
            NE_REFLECT_FIELD(lineSpacing),
            NE_REFLECT_FIELD(characterSpacing),
            NE_REFLECT_FIELD(wordSpacing),
            NE_REFLECT_FIELD(paragraphSpacing),
            NE_REFLECT_FIELD(richText),
            NE_REFLECT_FIELD(materialUUID),
            NE_REFLECT_FIELD(raycastTarget)
            NE_REFLECT_END()
    };

}
#endif // END UI_TEXT_HPP