#include "pch.h"
#include "SceneLightmapRuntime.hpp"

#include "Scene.hpp"
#include "Core/SpdLogger.hpp"
#include "Lighting/LightingAsset.hpp"
#include "ResourceManagement/ResourceManager.hpp"

#include <cmath>
#include <limits>
#include <mutex>

namespace NE::SceneManagement {
    namespace {
        bool IsPageRecordResolvable(const Lighting::LightmapPageRecord& page) {
            if (page.pageId.empty()) {
                return false;
            }

            if (page.width == 0 || page.height == 0) {
                return false;
            }

            if (page.irradianceTextureUUID.empty()) {
                return false;
            }

            if (page.pageType == Lighting::LightmapPageType::Directional &&
                page.directionTextureUUID.empty()) {
                return false;
            }

            return true;
        }

        std::mutex g_lightmapStateMutex;
        std::unordered_map<const Scene*, LightmapRuntimeState> g_lightmapStates;

        LightmapRuntimeState BuildState(Scene& scene) {
            LightmapRuntimeState state{};
            auto& container = scene.GetLightingContainer();

            container.resolvedAsset.reset();
            container.pageIdToSlot.clear();
            container.statusMessage.clear();

            state.containerEnabled = container.enabled;
            state.containerValid = false;
            state.lightingAssetRef = container.lightingAssetRef;
            state.lightingRevisionId = container.lightingRevisionId;
            state.dependencySignature = container.dependencySignature;

            if (!state.containerEnabled) {
                state.failureReason = "scene lighting disabled";
                container.valid = false;
                container.statusMessage = state.failureReason;
                return state;
            }

            if (state.lightingAssetRef.empty()) {
                state.failureReason = "missing lighting asset reference";
                container.valid = false;
                container.statusMessage = state.failureReason;
                return state;
            }

            auto lightingAsset = Resource::ResourceManager::GetInstance().LoadResource<Lighting::LightingAsset>(state.lightingAssetRef);
            if (!lightingAsset) {
                state.failureReason = "failed to load lighting asset sidecar";
                container.valid = false;
                container.statusMessage = state.failureReason;
                return state;
            }

            if (lightingAsset->GetFormatVersionMajor() != 1) {
                state.failureReason = "unsupported lighting asset major version";
                container.valid = false;
                container.statusMessage = state.failureReason;
                return state;
            }

            if (!state.lightingRevisionId.empty() &&
                lightingAsset->GetLightingRevisionId() != state.lightingRevisionId) {
                state.failureReason = "lighting revision mismatch";
                container.valid = false;
                container.statusMessage = state.failureReason;
                return state;
            }

            if (!state.dependencySignature.empty() &&
                lightingAsset->GetDependencySignature() != state.dependencySignature) {
                state.failureReason = "lighting dependency signature mismatch";
                container.valid = false;
                container.statusMessage = state.failureReason;
                return state;
            }

            const auto& pages = lightingAsset->GetPages();
            std::uint32_t slot = 0;
            std::size_t invalidPageCount = 0;
            for (const auto& page : pages) {
                if (!IsPageRecordResolvable(page)) {
                    ++invalidPageCount;
                    SPD_WARNING("Skipping invalid lightmap page while resolving scene lighting: pageId='"
                        << page.pageId << "'");
                    ++slot;
                    continue;
                }

                const auto [it, inserted] = state.pageSlots.emplace(page.pageId, slot);
                if (!inserted) {
                    ++invalidPageCount;
                    SPD_WARNING("Skipping duplicate lightmap page id while resolving scene lighting: '"
                        << page.pageId << "'");
                }

                ++slot;
            }

            state.manifestResolved = !state.pageSlots.empty();
            if (!state.manifestResolved) {
                state.failureReason = "lighting asset has no resolvable pages";
                container.valid = false;
                container.statusMessage = state.failureReason;
                return state;
            }

            state.containerValid = true;
            state.lightingUsable = true;
            container.valid = true;
            container.resolvedAsset = std::move(lightingAsset);
            container.pageIdToSlot = state.pageSlots;
            container.statusMessage = invalidPageCount > 0 ? "ready with skipped pages" : "ready";
            return state;
        }
    }

    const LightmapRuntimeState* GetSceneLightmapRuntimeState(const Scene* scene) {
        if (!scene) return nullptr;

        std::scoped_lock lock(g_lightmapStateMutex);
        auto it = g_lightmapStates.find(scene);
        return (it != g_lightmapStates.end()) ? &it->second : nullptr;
    }

    void ClearSceneLightmapRuntimeState(const Scene* scene) {
        if (!scene) return;

        auto& container = const_cast<Scene*>(scene)->GetLightingContainer();
        container.resolvedAsset.reset();
        container.pageIdToSlot.clear();
        container.statusMessage.clear();
        container.valid = false;

        std::scoped_lock lock(g_lightmapStateMutex);
        g_lightmapStates.erase(scene);
    }

    void ResolveSceneLightmapRuntimeState(Scene& scene) {
        LightmapRuntimeState state = BuildState(scene);

        if (state.containerEnabled && !state.lightingUsable && !state.failureReason.empty()) {
            SPD_WARNING("Scene lightmap runtime resolution fell back: " << state.failureReason);
        }

        std::scoped_lock lock(g_lightmapStateMutex);
        g_lightmapStates[&scene] = std::move(state);
    }

    bool TryResolveSceneLightmapPageSlot(const Scene& scene, const std::string& pageId, std::uint32_t& outPageSlot) {
        outPageSlot = std::numeric_limits<std::uint32_t>::max();
        if (pageId.empty()) return false;

        std::scoped_lock lock(g_lightmapStateMutex);
        auto itState = g_lightmapStates.find(&scene);
        if (itState == g_lightmapStates.end()) return false;

        const auto& state = itState->second;
        if (!state.lightingUsable) return false;

        const auto itPage = state.pageSlots.find(pageId);
        if (itPage == state.pageSlots.end()) return false;

        outPageSlot = itPage->second;
        return true;
    }

    bool IsFiniteLightmapTransform(const Math::Vec2& scale, const Math::Vec2& offset) {
        return std::isfinite(scale.x) &&
            std::isfinite(scale.y) &&
            std::isfinite(offset.x) &&
            std::isfinite(offset.y) &&
            scale.x > 0.0f &&
            scale.y > 0.0f;
    }
}
