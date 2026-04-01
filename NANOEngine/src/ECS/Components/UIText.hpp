#ifndef UI_TEXT_HPP
#define UI_TEXT_HPP

#include <string>
#include <cstdint>
#include "Math/Vec2.hpp"
#include "Math/Vec4.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIText {
        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;

        std::string text = "New Text";
        std::string fontUUID;
        float fontSize = 16.0f;
        NE::Math::Vec4 color{ 0.0f, 0.0f, 0.0f, 1.0f };

        enum class Alignment { LEFT, CENTER, RIGHT };
        Alignment horizontalAlign = Alignment::LEFT;

        enum class VerticalAlignment { TOP, MIDDLE, BOTTOM };
        VerticalAlignment verticalAlign = VerticalAlignment::TOP;

        bool wordWrap = false;
        float lineSpacing = 1.0f;    // Line height multiplier (1.0 = normal, like Unity lineSpacing)
        bool richText = false;       // Enable <color>, <size>, <b>, <i> tag parsing

        // Auto-scaling (like Unity's "Best Fit")
        bool autoScale = false;
        float minFontSize = 10.0f;
        float maxFontSize = 100.0f;

        // === RUNTIME FIELDS (not serialized) ===
        bool isDirty = true;
        NE::Math::Vec2 cachedSize{ 0.0f, 0.0f };  // Written by UIRenderSystem; read by UILayoutSystem

        // Reflection - only serialize user-editable fields
        NE_REFLECT_BEGIN(UIText)
            NE_REFLECT_FIELD(luid),
            NE_REFLECT_FIELD(text),
            NE_REFLECT_FIELD(fontUUID),
            NE_REFLECT_FIELD(fontSize),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD(horizontalAlign),
            NE_REFLECT_FIELD(verticalAlign),
            NE_REFLECT_FIELD(wordWrap),
            NE_REFLECT_FIELD(lineSpacing),
            NE_REFLECT_FIELD(richText),
            NE_REFLECT_FIELD(autoScale),
            NE_REFLECT_FIELD(minFontSize),
            NE_REFLECT_FIELD(maxFontSize)
        NE_REFLECT_END()
    };
} // namespace NE::ECS::Component
#endif // END UI_TEXT_HPP
