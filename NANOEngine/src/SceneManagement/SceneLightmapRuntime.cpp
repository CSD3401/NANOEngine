#include "pch.h"
#include "SceneLightmapRuntime.hpp"

#include "Scene.hpp"
#include "Core/SpdLogger.hpp"
#include "Graphics/OpenGL/GLTexture.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>

namespace NE::SceneManagement {
    namespace {
        void LogLightmapRuntimeWarning(const std::string& message) {
#ifndef PRODUCTION_BUILD
            SPD_WARNING(message);
#else
            (void)message;
#endif
        }

        void NoteResolvedPage(LightmapRuntimeState& state) {
#ifndef PRODUCTION_BUILD
            ++state.debugStats.resolvedPageCount;
#else
            (void)state;
#endif
        }

        void NoteFailedPageResolve(LightmapRuntimeState& state) {
#ifndef PRODUCTION_BUILD
            ++state.debugStats.failedPageResolveCount;
#else
            (void)state;
#endif
        }

        std::mutex g_lightmapStateMutex;
        std::unordered_map<const Scene*, LightmapRuntimeState> g_lightmapStates;

        void ReleasePreviewResidentHandles(LightmapRuntimeState& state) {
#ifndef PRODUCTION_BUILD
            if (!state.previewOverrideActive) {
                state.previewResidentHandles.clear();
                return;
            }

            for (const std::uint64_t handle : state.previewResidentHandles) {
                if (handle != 0u) {
                    glMakeTextureHandleNonResidentARB(handle);
                }
            }
            state.previewResidentHandles.clear();
            state.previewOverrideActive = false;
#else
            state.previewResidentHandles.clear();
            state.previewOverrideActive = false;
#endif
        }

        void PopulateFallbackHandles(LightmapRuntimeState& state) {
            std::uint64_t fallbackHandle = 0;
            for (const std::uint64_t handle : state.irradianceHandles) {
                if (handle != 0u) {
                    fallbackHandle = handle;
                    break;
                }
            }

            if (fallbackHandle == 0u) {
                return;
            }

            for (auto& handle : state.irradianceHandles) {
                if (handle == 0u) {
                    handle = fallbackHandle;
                }
            }
        }

        void SetFailure(Scene& scene, LightmapRuntimeState& state, const std::string& reason) {
            auto& container = scene.GetLightingContainer();
            state.failureReason = reason;
            container.valid = false;
            container.statusMessage = reason;
        }

        LightmapRuntimeState BuildState(Scene& scene) {
            LightmapRuntimeState state{};
            auto& container = scene.GetLightingContainer();

            container.resolvedAsset.reset();
            container.pageIdToSlot.clear();
            container.statusMessage.clear();

            state.containerEnabled = container.enabled;
            state.lightingAssetRef = container.lightingAssetRef;
            state.lightingRevisionId = container.lightingRevisionId;
            state.dependencySignature = container.dependencySignature;

            if (!state.containerEnabled) {
                SetFailure(scene, state, "scene lighting disabled");
                return state;
            }

            if (state.lightingAssetRef.empty()) {
                SetFailure(scene, state, "missing lighting asset reference");
                return state;
            }

            auto lightingAsset = Resource::ResourceManager::GetInstance().LoadResource<Lighting::LightmapResource>(state.lightingAssetRef);
            if (!lightingAsset) {
                SetFailure(scene, state, "failed to load lighting asset sidecar");
                return state;
            }

            if (lightingAsset->GetFormatVersionMajor() != 2) {
                SetFailure(scene, state, "unsupported lighting asset major version");
                return state;
            }

            if (!state.lightingRevisionId.empty() &&
                lightingAsset->GetLightingRevisionId() != state.lightingRevisionId) {
                SetFailure(scene, state, "lighting revision mismatch");
                return state;
            }

            if (!state.dependencySignature.empty() &&
                lightingAsset->GetDependencySignature() != state.dependencySignature) {
                SetFailure(scene, state, "lighting dependency signature mismatch");
                return state;
            }

            state.pageSlots = lightingAsset->GetPageSlots();
            state.manifestResolved = lightingAsset->IsManifestResolved();
            state.lightingUsable = lightingAsset->IsUsable();
            state.irradianceHandles = lightingAsset->GetIrradianceHandles();
            state.pages.resize(lightingAsset->GetPages().size());

            std::size_t skippedPageCount = 0u;
            const auto& pages = lightingAsset->GetPages();
            for (std::size_t slot = 0; slot < pages.size(); ++slot) {
                const auto& page = pages[slot];
                auto& runtimePage = state.pages[slot];
                runtimePage.pageId = page.manifest.pageId;
                runtimePage.pageType = page.manifest.pageType;
                runtimePage.irradianceTextureUUID = page.manifest.irradianceTextureUUID;
                runtimePage.width = page.manifest.width;
                runtimePage.height = page.manifest.height;
                runtimePage.irradianceTexture = page.irradianceTexture;
                runtimePage.irradianceHandle = page.irradianceHandle;
                runtimePage.resolved = page.resolved;
                runtimePage.failureReason = page.failureReason;

                if (runtimePage.resolved) {
                    NoteResolvedPage(state);
                } else {
                    ++skippedPageCount;
                    NoteFailedPageResolve(state);
                    if (!runtimePage.pageId.empty() && !runtimePage.failureReason.empty()) {
                        LogLightmapRuntimeWarning(
                            "Skipping lightmap page '" + runtimePage.pageId + "' while resolving scene lighting: " +
                            runtimePage.failureReason);
                    }
                }
            }

            if (!state.manifestResolved) {
                SetFailure(scene, state,
                    lightingAsset->GetFailureReason().empty()
                    ? "lighting asset has no resolvable pages"
                    : lightingAsset->GetFailureReason());
                return state;
            }

            state.containerValid = true;
            container.valid = state.lightingUsable;
            container.resolvedAsset = std::move(lightingAsset);
            container.pageIdToSlot = state.pageSlots;

            if (!state.lightingUsable) {
                SetFailure(scene, state,
                    container.resolvedAsset && !container.resolvedAsset->GetFailureReason().empty()
                    ? container.resolvedAsset->GetFailureReason()
                    : "lighting asset resolved no usable runtime pages");
                return state;
            }

            container.statusMessage = skippedPageCount > 0 ? "ready with skipped pages" : "ready";
            return state;
        }
    }

    void ResetSceneLightmapDebugStats(Scene& scene) {
#ifndef PRODUCTION_BUILD
        std::scoped_lock lock(g_lightmapStateMutex);
        auto it = g_lightmapStates.find(&scene);
        if (it == g_lightmapStates.end()) return;

        auto& stats = it->second.debugStats;
        stats.lightmappedDrawCount = 0;
        stats.skippedMissingUv1Count = 0;
        stats.skippedInvalidBindingCount = 0;
        stats.skippedInvalidTransformCount = 0;
        stats.skippedMissingPageCount = 0;
#else
        (void)scene;
#endif
    }

    void SetSceneLightmapDebugStats(Scene& scene, const LightmapRuntimeDebugStats& stats) {
#ifndef PRODUCTION_BUILD
        std::scoped_lock lock(g_lightmapStateMutex);
        auto it = g_lightmapStates.find(&scene);
        if (it == g_lightmapStates.end()) return;

        auto& dst = it->second.debugStats;
        dst.lightmappedDrawCount = stats.lightmappedDrawCount;
        dst.skippedMissingUv1Count = stats.skippedMissingUv1Count;
        dst.skippedInvalidBindingCount = stats.skippedInvalidBindingCount;
        dst.skippedInvalidTransformCount = stats.skippedInvalidTransformCount;
        dst.skippedMissingPageCount = stats.skippedMissingPageCount;
#else
        (void)scene;
        (void)stats;
#endif
    }

    bool EmitSceneLightmapWarningOnce(Scene& scene, std::string key, const std::string& message) {
#ifndef PRODUCTION_BUILD
        bool inserted = false;
        {
            std::scoped_lock lock(g_lightmapStateMutex);
            auto it = g_lightmapStates.find(&scene);
            if (it == g_lightmapStates.end()) return false;
            inserted = it->second.emittedWarningKeys.emplace(std::move(key)).second;
        }

        if (inserted) {
            SPD_WARNING(message);
        }
        return inserted;
#else
        (void)scene;
        (void)key;
        (void)message;
        return false;
#endif
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
        auto it = g_lightmapStates.find(scene);
        if (it != g_lightmapStates.end()) {
            ReleasePreviewResidentHandles(it->second);
            g_lightmapStates.erase(it);
        }
    }

    void SetSceneLightmapPreviewState(Scene& scene, const std::vector<LightmapRuntimePreviewPageInput>& pages) {
#ifndef PRODUCTION_BUILD
        LightmapRuntimeState state{};
        auto& container = scene.GetLightingContainer();

        state.containerEnabled = true;
        state.containerValid = false;
        state.manifestResolved = false;
        state.lightingUsable = false;
        state.previewOverrideActive = true;
        state.pages.resize(pages.size());
        state.irradianceHandles.assign(
            std::min<std::size_t>(pages.size(), static_cast<std::size_t>(kMaxSceneLightmapPages)),
            0ull);

        std::size_t skippedPageCount = 0;
        for (std::uint32_t slot = 0; slot < pages.size(); ++slot) {
            const auto& inputPage = pages[slot];
            auto& runtimePage = state.pages[slot];
            runtimePage.pageId = inputPage.pageId;
            runtimePage.pageType = Lighting::LightmapPageType::NonDirectional;
            runtimePage.width = inputPage.width;
            runtimePage.height = inputPage.height;

            if (inputPage.pageId.empty()) {
                ++skippedPageCount;
                NoteFailedPageResolve(state);
                runtimePage.failureReason = "missing page id";
                continue;
            }

            const auto [_, inserted] = state.pageSlots.emplace(inputPage.pageId, slot);
            if (!inserted) {
                ++skippedPageCount;
                NoteFailedPageResolve(state);
                runtimePage.failureReason = "duplicate page id";
                continue;
            }

            if (slot >= kMaxSceneLightmapPages) {
                ++skippedPageCount;
                NoteFailedPageResolve(state);
                runtimePage.failureReason = "page exceeds runtime slot limit";
                continue;
            }

            if (inputPage.width == 0u || inputPage.height == 0u || inputPage.textureId == 0u) {
                ++skippedPageCount;
                NoteFailedPageResolve(state);
                runtimePage.failureReason = "invalid preview texture input";
                continue;
            }

            runtimePage.irradianceHandle =
                Graphics::OpenGL::GetClampBindlessHandleForTexture(inputPage.textureId);
            runtimePage.resolved = (runtimePage.irradianceHandle != 0u);
            if (!runtimePage.resolved) {
                ++skippedPageCount;
                NoteFailedPageResolve(state);
                runtimePage.failureReason = "failed to create preview bindless handle";
                continue;
            }

            state.previewResidentHandles.push_back(runtimePage.irradianceHandle);
            state.irradianceHandles[slot] = runtimePage.irradianceHandle;
            NoteResolvedPage(state);
        }

        PopulateFallbackHandles(state);

        state.manifestResolved = !state.pageSlots.empty();
        state.containerValid = state.manifestResolved;
        state.lightingUsable = std::any_of(
            state.irradianceHandles.begin(),
            state.irradianceHandles.end(),
            [](std::uint64_t handle) { return handle != 0u; });
        if (!state.manifestResolved) {
            state.failureReason = "preview bake produced no resolvable pages";
        } else if (!state.lightingUsable) {
            state.failureReason = "preview bake produced no usable runtime pages";
        }

        container.resolvedAsset.reset();
        container.pageIdToSlot = state.pageSlots;
        container.valid = state.lightingUsable;
        container.statusMessage = state.lightingUsable
            ? (skippedPageCount > 0 ? "preview ready with skipped pages" : "preview ready")
            : (state.failureReason.empty() ? "preview unavailable" : state.failureReason);

        std::scoped_lock lock(g_lightmapStateMutex);
        auto it = g_lightmapStates.find(&scene);
        if (it != g_lightmapStates.end()) {
            ReleasePreviewResidentHandles(it->second);
        }
        g_lightmapStates[&scene] = std::move(state);
#else
        (void)scene;
        (void)pages;
#endif
    }

    void ClearSceneLightmapPreviewState(Scene& scene) {
        ResolveSceneLightmapRuntimeState(scene);
    }

    void ResolveSceneLightmapRuntimeState(Scene& scene) {
        LightmapRuntimeState state = BuildState(scene);

        if (state.containerEnabled && !state.lightingUsable && !state.failureReason.empty()) {
            LogLightmapRuntimeWarning("Scene lightmap runtime resolution fell back: " + state.failureReason);
        }

        std::scoped_lock lock(g_lightmapStateMutex);
        auto it = g_lightmapStates.find(&scene);
        if (it != g_lightmapStates.end()) {
            ReleasePreviewResidentHandles(it->second);
        }
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
        if (itPage->second >= state.pages.size()) return false;

        const auto& runtimePage = state.pages[itPage->second];
        if (!runtimePage.resolved || runtimePage.irradianceHandle == 0) return false;

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
