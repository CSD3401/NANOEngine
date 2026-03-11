#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "NANOEngineAPI.hpp"
#include "Lighting/LightingAsset.hpp"
#include "Math/Vec2.hpp"

namespace NE::Graphics::OpenGL {
    class GLTexture;
}

namespace NE::SceneManagement {
    class Scene;

    inline constexpr std::uint32_t kMaxSceneLightmapPages = 128u;

    struct LightmapRuntimePreviewPageInput {
        std::string pageId;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        unsigned int textureId = 0;
    };

    struct LightmapRuntimePage {
        std::string pageId;
        Lighting::LightmapPageType pageType = Lighting::LightmapPageType::NonDirectional;
        std::string irradianceTextureUUID;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::shared_ptr<Graphics::OpenGL::GLTexture> irradianceTexture;
        std::uint64_t irradianceHandle = 0;
        bool resolved = false;
        std::string failureReason;
    };

    struct LightmapRuntimeDebugStats {
        std::size_t resolvedPageCount = 0;
        std::size_t failedPageResolveCount = 0;
        std::size_t lightmappedDrawCount = 0;
        std::size_t skippedMissingUv1Count = 0;
        std::size_t skippedInvalidBindingCount = 0;
        std::size_t skippedInvalidTransformCount = 0;
        std::size_t skippedMissingPageCount = 0;
    };

    struct LightmapRuntimeState {
        bool containerEnabled = false;
        bool containerValid = false;
        bool manifestResolved = false;
        bool lightingUsable = false;
        std::string lightingAssetRef;
        std::string lightingRevisionId;
        std::string dependencySignature;
        std::string failureReason;
        bool previewOverrideActive = false;
        std::unordered_map<std::string, std::uint32_t> pageSlots;
        std::vector<LightmapRuntimePage> pages;
        std::vector<std::uint64_t> irradianceHandles;
        std::vector<std::uint64_t> previewResidentHandles;
        LightmapRuntimeDebugStats debugStats;
        std::unordered_set<std::string> emittedWarningKeys;
    };

    NANOENGINE_API void ResetSceneLightmapDebugStats(Scene& scene);
    NANOENGINE_API void SetSceneLightmapDebugStats(Scene& scene, const LightmapRuntimeDebugStats& stats);
    NANOENGINE_API bool EmitSceneLightmapWarningOnce(Scene& scene, std::string key, const std::string& message);
    NANOENGINE_API const LightmapRuntimeState* GetSceneLightmapRuntimeState(const Scene* scene);
    NANOENGINE_API void ClearSceneLightmapRuntimeState(const Scene* scene);
    NANOENGINE_API void SetSceneLightmapPreviewState(Scene& scene, const std::vector<LightmapRuntimePreviewPageInput>& pages);
    NANOENGINE_API void ClearSceneLightmapPreviewState(Scene& scene);
    NANOENGINE_API void ResolveSceneLightmapRuntimeState(Scene& scene);
    NANOENGINE_API bool TryResolveSceneLightmapPageSlot(const Scene& scene, const std::string& pageId, std::uint32_t& outPageSlot);
    NANOENGINE_API bool IsFiniteLightmapTransform(const Math::Vec2& scale, const Math::Vec2& offset);
}
