#pragma once

#include <variant>
#include <array>

#include "Math/Vec3.hpp"
#include "Math/Mat4.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    // Maybe add radius next time

	struct Light {
        static constexpr int DIR_CASCADES = 4;

        struct DirectionalLightData {
            float intensity = 1.f;
            NE_REFLECT_BEGIN(DirectionalLightData)
                NE_REFLECT_FIELD_NAMED(intensity, "Intensity")
                NE_REFLECT_END()
        };

        struct SpotLightData {
            float intensity = 1.f;
            float range = 10.f;
            float innerConeAngleDeg = 20.f;
            float outerConeAngleDeg = 30.f;

            NE_REFLECT_BEGIN(SpotLightData)
                NE_REFLECT_FIELD_NAMED(intensity, "Intensity"),
                NE_REFLECT_FIELD_NAMED(innerConeAngleDeg, "Inner Cone Angle"),
                NE_REFLECT_FIELD_NAMED(outerConeAngleDeg, "Outer Cone Angle"),
                NE_REFLECT_FIELD_NAMED(range, "Range")
            NE_REFLECT_END()
        };

        struct PointLightData {
            float intensity = 1.f;
            float range = 10.f;

            NE_REFLECT_BEGIN(PointLightData)
                NE_REFLECT_FIELD_NAMED(intensity, "Intensity"),
                NE_REFLECT_FIELD_NAMED(range, "Range")
                NE_REFLECT_END()
        };

        struct AreaLightData {
            float intensity = 1.f;
            float range = 10.f;
            float width = 20.f;
            float height = 50.f;

            NE_REFLECT_BEGIN(AreaLightData)
                NE_REFLECT_FIELD_NAMED(intensity, "Intensity"),
                NE_REFLECT_FIELD_NAMED(width, "Source Width"),
                NE_REFLECT_FIELD_NAMED(height, "Source Height"),
                NE_REFLECT_FIELD_NAMED(range, "Range")
                NE_REFLECT_END()
        };

        using LightTypeData = std::variant<
            DirectionalLightData,
            PointLightData,
            SpotLightData,
            AreaLightData
        >;

        enum Type : uint8_t {
            Directional,
            Point,
            Spot,
            Area
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
        Math::Vec3 direction{ 0.f, -1.f, 0.f };

        // Exposed Shared
        Math::Vec3 color{ 1.f,1.f,1.f };
        uint64_t luid = 0;
        Type type = Type::Directional;
        LightTypeData data;
        ShadowType shadowType = ShadowType::None;
        ShadowUpdateMode shadowUpdateMode = ShadowUpdateMode::NoneUpdate;
        bool isDirty = false;

        uint32_t shadowMapTex = 0;
        uint32_t shadowMapFBO = 0;
        Math::Mat4 lightViewProj{};
        int shadowIndex = -1;
        bool shadowBaked = false;

        std::array<uint32_t, DIR_CASCADES> dirShadowTex{};  // 0-init
        std::array<uint32_t, DIR_CASCADES> dirShadowFBO{};  // 0-init
        std::array<Math::Mat4, DIR_CASCADES> dirLightVP{};
        std::array<float, DIR_CASCADES> dirCascadeSplitsVS{}; // view-space split depths (positive, in world units)
        uint8_t shadowCascadeCount = 0;
        int dirShadowRes[DIR_CASCADES] = { 0,0,0,0 };

        NE_REFLECT_BEGIN(Light)
            NE_REFLECT_FIELD_HIDDEN(type),
            NE_REFLECT_FIELD_HIDDEN(data),
            NE_REFLECT_FIELD(color),
            NE_REFLECT_FIELD_HIDDEN(shadowType),
            NE_REFLECT_FIELD_HIDDEN(shadowUpdateMode)
            NE_REFLECT_END()
	};

}