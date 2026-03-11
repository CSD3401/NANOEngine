#pragma once

#include <array>
#include <cstdint>

#include "ECS/Core/Entity.hpp"
#include "ECS/Components/Light.hpp"
#include "Math/Mat4.hpp"

namespace NE::Graphics {
    struct RenderLightRef {
        ECS::Entity entity = ECS::NO_ENTITY;
        ECS::Component::Light* light = nullptr;
    };

    struct LightShadowRuntime {
        static constexpr int DIR_CASCADES = ECS::Component::Light::DIR_CASCADES;

        uint32_t shadowMapTex = 0;
        uint32_t shadowMapFBO = 0;
        int shadowMapRes = 0;
        Math::Mat4 lightViewProj{};
        int shadowIndex = -1;
        bool shadowBaked = false;

        std::array<uint32_t, DIR_CASCADES> dirShadowTex{};
        std::array<uint32_t, DIR_CASCADES> dirShadowFBO{};
        std::array<Math::Mat4, DIR_CASCADES> dirLightVP{};
        std::array<float, DIR_CASCADES> dirCascadeSplitsVS{};
        uint8_t shadowCascadeCount = 0;
        std::array<int, DIR_CASCADES> dirShadowRes{};
    };
}
