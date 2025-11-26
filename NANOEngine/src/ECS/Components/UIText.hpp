#ifndef UI_TEXT_HPP
#define UI_TEXT_HPP

#include <string>
#include <filesystem>
#include "../../Math/Vec4.hpp"

namespace NE::ECS::Component {
    struct UIText {
        std::string text = "New Text";
        std::filesystem::path fontPath;
        float fontSize = 16.0f;
        NE::Math::Vec4 color{ 0.0f, 0.0f, 0.0f, 1.0f };

        enum class Alignment { LEFT, CENTER, RIGHT };
        Alignment horizontalAlign = Alignment::LEFT;

        enum class VerticalAlignment { TOP, MIDDLE, BOTTOM };
        VerticalAlignment verticalAlign = VerticalAlignment::TOP;

        bool wordWrap = false;
    };
} // namespace NE::ECS::Component
#endif // END UI_TEXT_HPP
