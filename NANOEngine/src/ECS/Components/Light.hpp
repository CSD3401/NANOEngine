#pragma once

#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

	struct Light {
        enum Type : uint8_t {
            Directional,
            Point,
            Spot
        };

        enum ShadowType : uint8_t {
            None,
            Hard,
            Soft
        };

        enum ShadowUpdateMode : uint8_t {
            NoneUpdate,
            Realtime,
            StaticBake
        };
        
        // Internal (Set by transform component)
        Math::Vec3 position{ 0.f, 0.f, 0.f };

        // Exposed Shared
        Type type = Type::Directional;
        ShadowType shadowType = ShadowType::None;
        ShadowUpdateMode shadowUpdateMode = ShadowUpdateMode::NoneUpdate;
        Math::Vec3 color{ 1.f,1.f,1.f };
        float intensity{ 1.f };
        
        // Directional
        Math::Vec3 direction{ 0.f, -1.f, 0.f };

        // Spot Light
        float innerCutoff{ 0.91f };
        float outerCutoff{ 0.82f };
		float radius{ 10.f };

        float constant{ 1.f };
        float linear{ 0.f };
        float quadratic{ 1.f };

        uint64_t luid;

        // Dirty flag for editor changes
        bool isDirty = false;

        // --- Runtime shadow data (not serialized) ---
        uint32_t shadowMapTex = 0;
        uint32_t shadowMapFBO = 0;
        int shadowIndex = -1;
        Math::Mat4 lightViewProj{};
        bool shadowBaked = false;

        NE_REFLECT_BEGIN(Light)
            //NE_REFLECT_FIELD(type),
            NE_REFLECT_FIELD(direction),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD(intensity),
            NE_REFLECT_FIELD(innerCutoff),
            NE_REFLECT_FIELD(outerCutoff),
			NE_REFLECT_FIELD(radius),
            NE_REFLECT_FIELD(constant),
            NE_REFLECT_FIELD(linear),
            NE_REFLECT_FIELD(quadratic),
            NE_REFLECT_FIELD_HIDDEN(shadowType),
            NE_REFLECT_FIELD_HIDDEN(shadowUpdateMode)
            NE_REFLECT_END()
	};

}