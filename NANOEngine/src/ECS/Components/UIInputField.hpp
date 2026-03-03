#ifndef UI_INPUT_FIELD_HPP
#define UI_INPUT_FIELD_HPP

#include <string>
#include <cstdint>
#include "Math/Vec4.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIInputField {

        // === Content types ===
        enum class ContentType { STANDARD, INTEGER, DECIMAL, ALPHA_NUMERIC, PASSWORD };
        enum class LineType { SINGLE_LINE, MULTI_LINE };

        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;

        // Text content
        std::string text;
        std::string placeholderText = "Enter text...";

        // Input configuration
        ContentType contentType = ContentType::STANDARD;
        LineType lineType = LineType::SINGLE_LINE;
        int characterLimit = 0;          // 0 = no limit
        char passwordChar = '*';

        bool interactable = true;
        bool readOnly = false;

        // Visual — background color states (applied to sibling UIImage)
        NE::Math::Vec4 normalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        NE::Math::Vec4 selectedColor{ 0.88f, 0.88f, 1.0f, 1.0f };
        NE::Math::Vec4 disabledColor{ 0.7f, 0.7f, 0.7f, 0.5f };

        // Text colors (applied to sibling UIText)
        NE::Math::Vec4 textColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        NE::Math::Vec4 placeholderColor{ 0.5f, 0.5f, 0.5f, 1.0f };

        // Cursor
        NE::Math::Vec4 cursorColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        float cursorWidth = 2.0f;
        float cursorBlinkRate = 0.53f;   // seconds per blink cycle

        // Selection highlight
        NE::Math::Vec4 selectionColor{ 0.24f, 0.47f, 0.95f, 0.4f };

        // Event IDs (for script binding)
        uint32_t onValueChangedEventId = 0;
        uint32_t onSubmitEventId = 0;

        // === RUNTIME FIELDS (not serialized) ===
        int cursorPosition = 0;
        int selectionStart = -1;         // -1 = no selection
        int selectionEnd = -1;
        bool isFocused = false;
        float cursorBlinkTimer = 0.0f;
        bool cursorVisible = true;
        std::string previousText;        // For change detection

        // Reflection — only serialize user-editable fields
        NE_REFLECT_BEGIN(UIInputField)
            NE_REFLECT_FIELD(luid),
            NE_REFLECT_FIELD(text),
            NE_REFLECT_FIELD(placeholderText),
            NE_REFLECT_FIELD(contentType),
            NE_REFLECT_FIELD(lineType),
            NE_REFLECT_FIELD(characterLimit),
            NE_REFLECT_FIELD(passwordChar),
            NE_REFLECT_FIELD(interactable),
            NE_REFLECT_FIELD(readOnly),
            NE_REFLECT_FIELD(normalColor),
            NE_REFLECT_FIELD(selectedColor),
            NE_REFLECT_FIELD(disabledColor),
            NE_REFLECT_FIELD(textColor),
            NE_REFLECT_FIELD(placeholderColor),
            NE_REFLECT_FIELD(cursorColor),
            NE_REFLECT_FIELD(cursorWidth),
            NE_REFLECT_FIELD(cursorBlinkRate),
            NE_REFLECT_FIELD(selectionColor),
            NE_REFLECT_FIELD(onValueChangedEventId),
            NE_REFLECT_FIELD(onSubmitEventId)
        NE_REFLECT_END()
    };

} // namespace NE::ECS::Component
#endif // UI_INPUT_FIELD_HPP
