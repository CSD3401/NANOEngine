#ifndef RectTransform_HPP
#define RectTransform_HPP

#include <cstdint>

#include "Math/Vec2.hpp"
#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

	struct RectTransform {
        uint64_t luid{};

        Math::Vec3 localPosition{ 0.f, 0.f, 0.f };
        Math::Vec3 localRotation{ 0.f, 0.f, 0.f };
        Math::Vec3 localScale{ 1.f, 1.f, 1.f };

        Math::Vec2 dimension{ 1920.f, 1080.f };
        Math::Vec2 pivot{ 0.5f, 0.5f };
        Math::Vec2 offsetMin{ 0.f, 0.f };
        Math::Vec2 offsetMax{ 0.f, 0.f };

        Math::Vec2 anchorMin{ 0.5f, 0.5f };
        Math::Vec2 anchorMax{ 0.5f, 0.5f };

        // runtime
        Math::Vec2 computedMin{ 0,0 };
        Math::Vec2 computedMax{ 0,0 };
        Math::Vec2 computedSize{ 0,0 };
        Math::Vec2 computedPivotPos{ 0,0 };

        Math::Mat4 localMatrix{};
        Math::Mat4 worldMatrix{};

        bool layoutDirty = true;
        bool worldDirty = true;

        bool isDirty = false;

        NE_REFLECT_BEGIN(RectTransform)
            NE_REFLECT_FIELD_HIDDEN(luid),
            NE_REFLECT_FIELD_NAMED(localPosition, "Position"),
            NE_REFLECT_FIELD_NAMED(localRotation, "Rotation"),
            NE_REFLECT_FIELD_NAMED(localScale, "Scale"),
            NE_REFLECT_FIELD(dimension),
            NE_REFLECT_FIELD(pivot),
            NE_REFLECT_FIELD(offsetMin),
            NE_REFLECT_FIELD(offsetMax),
            NE_REFLECT_FIELD(anchorMin),
            NE_REFLECT_FIELD(anchorMax)
            NE_REFLECT_END()
	};

}

#endif
