#ifndef UI_TEXT_HPP
#define UI_TEXT_HPP

#include <string>
#include <filesystem>
#include "../../Math/Vec4.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {
    struct UIText {
        // LUID for serialization
        uint64_t luid = 0;

        std::string text = "New Text";
        std::string fontUUID;  // UUID for font asset (instead of path)
        float fontSize = 16.0f;
        NE::Math::Vec4 color{ 0.0f, 0.0f, 0.0f, 1.0f };

        enum class Alignment { LEFT, CENTER, RIGHT };
        Alignment horizontalAlign = Alignment::LEFT;

        enum class VerticalAlignment { TOP, MIDDLE, BOTTOM };
        VerticalAlignment verticalAlign = VerticalAlignment::MIDDLE;

        bool wordWrap = false;
        bool richText = false;  // Support rich text formatting (future)

        // Runtime-only fields
        uint64_t fontHandle = 0;  // Bindless handle or similar for font texture

        // Reflection
        NE_REFLECT_BEGIN(UIText)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD(text),
            NE_REFLECT_FIELD(fontUUID),
            NE_REFLECT_FIELD(fontSize),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD(horizontalAlign),
            NE_REFLECT_FIELD(verticalAlign),
            NE_REFLECT_FIELD(wordWrap),
            NE_REFLECT_FIELD(richText)
        NE_REFLECT_END()
    };
} // namespace NE::ECS::Component
#endif // END UI_TEXT_HPP
