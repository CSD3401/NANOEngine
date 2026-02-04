#ifndef UI_TEXT_HPP
#define UI_TEXT_HPP

#include <string>
#include <filesystem>
#include <vector>
#include <cstdint>
#include "Math/Vec2.hpp"
#include "Math/Vec3.hpp"
#include "Math/Vec4.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct UITextVertex {
        float x, y, z;
        float u, v;
        float r, g, b, a;
    };

    struct UIText {
        // === SERIALIZED FIELDS ===
        std::string text = "New Text";
        std::filesystem::path fontPath;
        float fontSize = 16.0f;
        NE::Math::Vec4 color{ 0.0f, 0.0f, 0.0f, 1.0f };

        enum class Alignment { LEFT, CENTER, RIGHT };
        Alignment horizontalAlign = Alignment::LEFT;

        enum class VerticalAlignment { TOP, MIDDLE, BOTTOM };
        VerticalAlignment verticalAlign = VerticalAlignment::TOP;

        bool wordWrap = false;

        // === RUNTIME FIELDS (not serialized) ===
        uint64_t fontAtlasHandle = 0;
        bool isDirty = true;
        std::vector<UITextVertex> cachedVertices;
        std::string cachedText;
        float cachedFontSize = 0.0f;
        NE::Math::Vec3 cachedPos{ 0.0f, 0.0f, 0.0f };
        NE::Math::Vec2 cachedSize{ 0.0f, 0.0f };
        float cachedRotZ = 0.0f;
        bool hasCachedTransform = false;

        // Reflection - only serialize user-editable fields
        NE_REFLECT_BEGIN(UIText)
            NE_REFLECT_FIELD(text),
            NE_REFLECT_FIELD(fontPath),
            NE_REFLECT_FIELD(fontSize),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD(horizontalAlign),
            NE_REFLECT_FIELD(verticalAlign),
            NE_REFLECT_FIELD(wordWrap)
        NE_REFLECT_END()
    };
} // namespace NE::ECS::Component
#endif // END UI_TEXT_HPP
