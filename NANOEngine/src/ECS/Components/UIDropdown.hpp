#ifndef UI_DROPDOWN_HPP
#define UI_DROPDOWN_HPP

#include <string>
#include <vector>
#include <cstdint>
#include "Math/Vec4.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UIDropdown {

        // === SERIALIZED FIELDS ===
        uint64_t luid = 0;

        // Options list
        std::vector<std::string> options{ "Option A", "Option B", "Option C" };
        int selectedIndex = 0;

        // Entity references (wired in inspector or auto-created by Add Component)
        uint32_t captionTextEntity = UINT32_MAX;    // UIText showing selected option
        uint32_t optionsPanelEntity = UINT32_MAX;   // Panel container (hidden when collapsed)

        bool interactable = true;

        // Colors — dropdown button background (applied to sibling UIImage)
        NE::Math::Vec4 normalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        NE::Math::Vec4 highlightedColor{ 0.92f, 0.92f, 0.92f, 1.0f };
        NE::Math::Vec4 pressedColor{ 0.78f, 0.78f, 0.78f, 1.0f };
        NE::Math::Vec4 disabledColor{ 0.7f, 0.7f, 0.7f, 0.5f };

        // Colors — option items
        NE::Math::Vec4 optionNormalColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        NE::Math::Vec4 optionHighlightedColor{ 0.85f, 0.85f, 1.0f, 1.0f };

        // Event ID (for script binding)
        uint32_t onValueChangedEventId = 0;

        NE_REFLECT_BEGIN(UIDropdown)
            NE_REFLECT_FIELD(luid),
            NE_REFLECT_FIELD(options),
            NE_REFLECT_FIELD(selectedIndex),
            NE_REFLECT_FIELD(captionTextEntity),
            NE_REFLECT_FIELD(optionsPanelEntity),
            NE_REFLECT_FIELD(interactable),
            NE_REFLECT_FIELD(normalColor),
            NE_REFLECT_FIELD(highlightedColor),
            NE_REFLECT_FIELD(pressedColor),
            NE_REFLECT_FIELD(disabledColor),
            NE_REFLECT_FIELD(optionNormalColor),
            NE_REFLECT_FIELD(optionHighlightedColor),
            NE_REFLECT_FIELD(onValueChangedEventId)
        NE_REFLECT_END()

        // === RUNTIME FIELDS (not serialized) ===
        bool isExpanded = false;
        int hoveredOptionIndex = -1;    // -1 = no option hovered
        int previousSelectedIndex = 0;
    };

} // namespace NE::ECS::Component
#endif // UI_DROPDOWN_HPP
