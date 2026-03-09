#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "NANOEngineAPI.hpp"
#include "Math/Vec2.hpp"

namespace NE::SceneManagement {
    class Scene;

    struct LightmapRuntimeState {
        bool containerEnabled = false;
        bool containerValid = false;
        bool manifestResolved = false;
        bool lightingUsable = false;
        std::string lightingAssetRef;
        std::string lightingRevisionId;
        std::string dependencySignature;
        std::string failureReason;
        std::unordered_map<std::string, std::uint32_t> pageSlots;
    };

    NANOENGINE_API const LightmapRuntimeState* GetSceneLightmapRuntimeState(const Scene* scene);
    NANOENGINE_API void ClearSceneLightmapRuntimeState(const Scene* scene);
    NANOENGINE_API void ResolveSceneLightmapRuntimeState(Scene& scene);
    NANOENGINE_API bool TryResolveSceneLightmapPageSlot(const Scene& scene, const std::string& pageId, std::uint32_t& outPageSlot);
    NANOENGINE_API bool IsFiniteLightmapTransform(const Math::Vec2& scale, const Math::Vec2& offset);
}
