#include "pch.h"
#include <cmath>
#include <limits>

#include "RenderSystem.hpp"
#include "Core/Profiler.hpp"
#include "Core/LUIDGenerator.hpp"
#include "Core/LUIDRegistry.hpp"
#include "Engine.hpp"
#include "ECS/Core/ComponentManager.hpp"
#include "ECS/Core/EntityManager.hpp"
#include "ECS/Components/Renderer.hpp"
#include "ECS/Components/Transform.hpp"
#include "Graphics/Core/GraphicsManager.hpp"
#include "Graphics/Core/Material.hpp"
#include "Graphics/Core/DrawCommand.hpp"
#include "SceneManagement/SceneLightmapRuntime.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "ECS/Components/LightmapBinding.hpp"

namespace NE::ECS::Systems {
    namespace {
        void ResolveRendererResources(NE::ECS::Component::Renderer& renderer) {
            auto& resourceManager = Resource::ResourceManager::GetInstance();

            if (!renderer.materialUUID.empty()) {
                renderer.material = resourceManager.LoadResource<Graphics::Material>(renderer.materialUUID);
            }
            if (!renderer.material) {
                renderer.materialUUID = "nelitmat";
                renderer.material = resourceManager.LoadResource<Graphics::Material>("nelitmat");
            }

            if (!renderer.modelUUID.empty()) {
                renderer.model = resourceManager.LoadResource<Graphics::Model>(renderer.modelUUID);
            }
            if (!renderer.model) {
                renderer.modelUUID = "builtin:model/cube";
                renderer.model = resourceManager.LoadResource<Graphics::Model>("builtin:model/cube");
                renderer.subMeshIndex = 0;
            }

            if (renderer.model && !renderer.model->meshes.empty()) {
                if (renderer.subMeshIndex < 0 || renderer.subMeshIndex >= (int32_t)renderer.model->meshes.size()) {
                    renderer.subMeshIndex = 0;
                }
            } else {
                renderer.subMeshIndex = -1;
            }

            renderer.isDirty = false;
        }

        inline float MaxScaleAxis(const NE::Math::Mat4& M) {
            NE::Math::Vec3 x = { M.GetElement(0, 0), M.GetElement(1, 0), M.GetElement(2, 0) };
            NE::Math::Vec3 y = { M.GetElement(0, 1), M.GetElement(1, 1), M.GetElement(2, 1) };
            NE::Math::Vec3 z = { M.GetElement(0, 2), M.GetElement(1, 2), M.GetElement(2, 2) };

            float sx = x.Length();
            float sy = y.Length();
            float sz = z.Length();
            return std::max(sx, std::max(sy, sz));
        }

        inline NE::Math::Vec3 TransformPoint(const NE::Math::Mat4& M, const NE::Math::Vec3& p) {
            NE::Math::Vec4 v = M * NE::Math::Vec4(p.x, p.y, p.z, 1.0f);
            return { v.x, v.y, v.z };
        }

        inline bool IsFiniteMatrix(const NE::Math::Mat4& matrix) {
            for (float value : matrix.a) {
                if (!std::isfinite(value)) {
                    return false;
                }
            }

            return true;
        }

    }

    RenderSystem::RenderSystem(ComponentManager* cm, EntityManager* em, Core::LUIDRegistry* lr)
        : m_componentManager(cm), m_entityManager(em), m_luidRegistry(lr) {}

    void RenderSystem::OnEntityAdded(Entity entity) {
        auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);
        ResolveRendererResources(renderer);

        if (renderer.luid == 0)
            renderer.luid = Core::LUIDGenerator::Generate("rd");

        m_luidRegistry->Register(renderer.luid, &renderer, entity);
    }

    void RenderSystem::OnEntityRemoved(Entity e) {
        auto& renderer = m_componentManager->GetComponent<Component::Renderer>(e);
        m_luidRegistry->Unregister(renderer.luid);
    }

    void RenderSystem::OnEntityActive(Entity /*entity*/) {}
    void RenderSystem::OnEntityInactive(Entity /*entity*/) {}

    void RenderSystem::Init() {
    }

    void RenderSystem::Update(double /*deltaTime*/) {
#ifndef PRODUCTION_BUILD
		NE_PROFILE_FUNCTION();
#endif
        const auto& entities = m_entities.GetDenseContainer();
        auto& activeScene = NE::GetScene();
        const auto* lightmapState = SceneManagement::GetSceneLightmapRuntimeState(&activeScene);
        SceneManagement::LightmapRuntimeDebugStats frameLightmapStats{};
        if (lightmapState) {
            frameLightmapStats.resolvedPageCount = lightmapState->debugStats.resolvedPageCount;
            frameLightmapStats.failedPageResolveCount = lightmapState->debugStats.failedPageResolveCount;
        }

        for (Entity entity : entities) {
            if (!m_entityManager->GetActive(entity)) continue;

            auto& renderer = m_componentManager->GetComponent<Component::Renderer>(entity);
            if (renderer.isDirty) {
                ResolveRendererResources(renderer);
            }

            if (!renderer.model) continue;
            if (renderer.subMeshIndex < 0 || renderer.subMeshIndex >= (int32_t)renderer.model->meshes.size()) continue;

            auto& transform = m_componentManager->GetComponent<Component::Transform>(entity);

            const auto& ms = renderer.model->meshes[renderer.subMeshIndex].localSphere;
            float baseR = ms.radius;

            Graphics::DrawCommand cmd;
            cmd.mesh = renderer.model->meshes[renderer.subMeshIndex].buffer;
            cmd.material = renderer.material;
            cmd.transform = transform.worldMatrix;

            float r = (float)(entity & 0xFF) / 255.0f;
            float g = (float)((entity >> 8) & 0xFF) / 255.0f;
            float b = (float)((entity >> 16) & 0xFF) / 255.0f;
            cmd.idRGB = Math::Vec3{ r, g, b };

            cmd.boundsCenterWS = TransformPoint(transform.worldMatrix, ms.center);
            cmd.boundsRadiusWs = baseR * MaxScaleAxis(transform.worldMatrix);

            cmd.castsShadow = (renderer.shadowCastMode != Component::Renderer::ShadowCastMode::Off);
            cmd.receivesShadow = renderer.receiveShadows;
            cmd.hasUv1 = renderer.model->meshes[renderer.subMeshIndex].hasUv1;

            const bool hasLightmapBinding = m_componentManager->HasComponent<Component::LightmapBinding>(entity);
            if (hasLightmapBinding) {
                auto& binding = m_componentManager->GetComponent<Component::LightmapBinding>(entity);
                binding.pageResolved = false;
                binding.resolvedPageSlot = Component::INVALID_LIGHTMAP_PAGE_SLOT;
            }

            if (hasLightmapBinding) {
                auto& binding = m_componentManager->GetComponent<Component::LightmapBinding>(entity);
                if (binding.enabled) {
                    const Math::Vec2 uvScale = binding.uvScale;
                    const Math::Vec2 uvOffset = binding.uvOffset;
                    const std::string& pageId = binding.pageId;

                    if (!cmd.hasUv1) {
                        ++frameLightmapStats.skippedMissingUv1Count;
                        SceneManagement::EmitSceneLightmapWarningOnce(
                            activeScene,
                            "missing-uv1:" + std::to_string(entity),
                            "Skipping lightmap sampling for entity " + std::to_string(entity) +
                            " because the bound mesh has no UV1 channel.");
                    } else if (!IsFiniteMatrix(transform.worldMatrix)) {
                        ++frameLightmapStats.skippedInvalidTransformCount;
                        SceneManagement::EmitSceneLightmapWarningOnce(
                            activeScene,
                            "invalid-world-matrix:" + std::to_string(entity),
                            "Skipping lightmap sampling for entity " + std::to_string(entity) +
                            " because its world transform contains non-finite values.");
                    } else if (!lightmapState || !lightmapState->lightingUsable) {
                        ++frameLightmapStats.skippedMissingPageCount;
                    } else if (!SceneManagement::IsFiniteLightmapTransform(uvScale, uvOffset)) {
                        ++frameLightmapStats.skippedInvalidBindingCount;
                        SceneManagement::EmitSceneLightmapWarningOnce(
                            activeScene,
                            "invalid-lightmap-transform:" + std::to_string(entity),
                            "Skipping lightmap sampling for entity " + std::to_string(entity) +
                            " because its LightmapBinding atlas transform is invalid.");
                    } else if (pageId.empty()) {
                        ++frameLightmapStats.skippedInvalidBindingCount;
                        SceneManagement::EmitSceneLightmapWarningOnce(
                            activeScene,
                            "missing-lightmap-page-id:" + std::to_string(entity),
                            "Skipping lightmap sampling for entity " + std::to_string(entity) +
                            " because its LightmapBinding has no page id.");
                    } else {
                        cmd.lightmapEnabled = true;
                        cmd.lightmapUvScale = uvScale;
                        cmd.lightmapUvOffset = uvOffset;

                        std::uint32_t pageSlot = std::numeric_limits<std::uint32_t>::max();
                        if (lightmapState &&
                            lightmapState->lightingUsable &&
                            SceneManagement::TryResolveSceneLightmapPageSlot(activeScene, pageId, pageSlot)) {
                            binding.pageResolved = true;
                            binding.resolvedPageSlot = pageSlot;
                            cmd.lightmapPageSlot = pageSlot;
                            ++frameLightmapStats.lightmappedDrawCount;
                        } else {
                            ++frameLightmapStats.skippedMissingPageCount;
                            if (lightmapState && lightmapState->manifestResolved) {
                                SceneManagement::EmitSceneLightmapWarningOnce(
                                    activeScene,
                                    "missing-lightmap-page:" + pageId,
                                    "Skipping lightmap sampling because lightmap page '" + pageId +
                                    "' could not be resolved to a usable runtime texture.");
                            }
                        }
                    }
                }
            }
            Graphics::GraphicsManager::Submit(cmd);
        }

        SceneManagement::SetSceneLightmapDebugStats(activeScene, frameLightmapStats);
    }

    void RenderSystem::Exit()
    {
    }
}
