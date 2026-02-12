#include "GraphicsManager.hpp"
#include "PostProcessPipeline.hpp"

#include "EditorCamera.hpp"
#include "Skybox.hpp"
#include "Material.hpp"
#include "Model.hpp"
#include "DrawCommand.hpp"
#include "LightGizmoCommand.hpp"
#include "DrawQueue.hpp"
#include "RenderViewManager.hpp"
#include "RenderSettings.hpp"

#include "../../Math/Mat4.hpp"
#include "../../Math/Vec3.hpp"

#include "../Interfaces/IFrameBuffer.hpp"
#include "../Interfaces/IShader.hpp"
#include "../Interfaces/ICommandBuffer.hpp"
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "../Interfaces/IStateCache.hpp"
#include "../Interfaces/IClusteredLighting.hpp"

#include "../OpenGL/GLCommandBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"
#include "../OpenGL/GLTexture.hpp"
#include "../OpenGL/GLStateCache.hpp"
#include "../Core/Primitives.hpp"
#include "UIRenderer.hpp"
#include "glfw/glfw3.h"
#include "Core/SpdLogger.hpp"
#include "InstanceData.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include "../OpenGL/GLFrameBuffer.hpp"
#include "../OpenGL/GLClusteredLighting.hpp"

#include "GizmosRenderer.hpp"
#include "Graphics/DebugRenderer/DebugDrawSystem.hpp"
#include "../../Core/Logger.hpp"
#include "../../Core/Profiler.hpp"
#include "../../ECS/Components/Light.hpp"
#include "../../SceneManagement/Scene.hpp"
#include "../../ResourceManagement/ResourceManager.hpp"

#include <glad/glad.h>
#include <algorithm>
#include <array>
#include <cmath>

#include "Input/InputManager.hpp"
#include "ShadowRenderer.hpp"
#include "Frustum.hpp"


namespace NE::Graphics {
    namespace {
        constexpr float LIGHT_GIZMO_PIXEL_SIZE = 128.f;

        constexpr std::array<const char*, 4> LIGHT_GIZMO_ICON_UUIDS = {
            "nedirlight", // Directional
            "nedirlight", // Point
            "nespotlight", // Spot
            "nedirlight"  // Area
        };

        std::vector<LightGizmoCommand> s_LightGizmoQueue;
        std::shared_ptr<IGeometryBuffer> s_LightGizmoMesh;
        std::array<std::shared_ptr<Material>, 4> s_LightGizmoMaterials;

        inline size_t ToLightTypeIndex(ECS::Component::Light::Type type) {
            const uint8_t raw = static_cast<uint8_t>(type);
            return (raw < s_LightGizmoMaterials.size()) ? static_cast<size_t>(raw) : static_cast<size_t>(0);
        }

        inline bool IsPerspectiveProjection(const Mat4& projection) {
            return std::abs(projection.GetElement(3, 3)) < 0.5f;
        }

        inline float ComputeWorldSizeForPixels(float pixelSize, float distanceToCamera, const Mat4& projection, float viewportHeight) {
            float projY = std::abs(projection.GetElement(1, 1));
            if (projY < 1e-4f) projY = 1.0f;

            if (IsPerspectiveProjection(projection)) {
                const float safeDistance = std::max(distanceToCamera, 0.001f);
                return pixelSize * (2.0f * safeDistance) / (projY * viewportHeight);
            }

            return pixelSize * (2.0f) / (projY * viewportHeight);
        }

        inline Mat4 BuildBillboardMatrix(const Vec3& position, const Vec3& right, const Vec3& up, float size) {
            Vec3 forward = right.Cross(up).Normalized();

            Mat4 model;
            model.SetToIdentity();

            model.GetElement(0, 0) = right.x * size;
            model.GetElement(1, 0) = right.y * size;
            model.GetElement(2, 0) = right.z * size;

            model.GetElement(0, 1) = up.x * size;
            model.GetElement(1, 1) = up.y * size;
            model.GetElement(2, 1) = up.z * size;

            model.GetElement(0, 2) = forward.x * size;
            model.GetElement(1, 2) = forward.y * size;
            model.GetElement(2, 2) = forward.z * size;

            model.SetTranslation(position);
            return model;
        }

        void InitializeLightGizmoResources() {
            auto quadModel = Resource::ResourceManager::GetInstance().LoadResource<Model>("builtin:model/quad");
            if (quadModel && !quadModel->meshes.empty()) {
                s_LightGizmoMesh = quadModel->meshes[0].buffer;
            } else {
                SPD_WARNING("Light gizmo mesh initialization failed: builtin quad model not available.");
                return;
            }

            auto baseMaterial = Resource::ResourceManager::GetInstance().LoadResource<Material>("neunlitmat");
            if (!baseMaterial) {
                SPD_WARNING("Light gizmo material initialization failed: neunlitmat not available.");
                return;
            }

            for (size_t i = 0; i < s_LightGizmoMaterials.size(); ++i) {
                auto material = std::make_shared<Material>(*baseMaterial);

                if (material->GetPipeline()) {
                    auto spec = material->GetPipeline()->GetSpecification();
                    spec.EnableBlending = true;
                    spec.EnableDepthTest = true;
                    spec.DepthWrite = false;
                    spec.CullMode = GL_NONE;
                    spec.PolygonMode = GL_FILL;
                    material->ApplyPipelineSpec(spec);
                }

                material->SetQueueBase(RenderQueue::OVERLAY);
                material->SetQueueOffset(0);
                material->SetUniformVec3("u_BaseColor", { 1.0f, 1.0f, 1.0f });
                material->SetUniformFloat("u_Opacity", 1.0f);
                material->SetUniformInt("u_AlphaClip", 1);
                material->SetUniformFloat("u_AlphaCutoff", 0.1f);

                material->SetUniformInt("h_HasAlbedoMap", 0);
                material->SetUniformInt("u_HasAlbedoMap", 0);
                material->SetUniformInt("h_HasOpacityMap", 0);
                material->SetUniformInt("u_HasOpacityMap", 0);
                material->m_Textures["u_AlbedoMap"] = nullptr;
                material->m_Textures["u_OpacityMap"] = nullptr;

                const char* iconUuid = LIGHT_GIZMO_ICON_UUIDS[i];
                if (iconUuid && iconUuid[0] != '\0') {
                    auto texture = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLTexture>(iconUuid);
                    if (texture) {
                        texture->MakeResident();
                        material->m_Textures["u_AlbedoMap"] = texture;
                        material->m_Textures["u_OpacityMap"] = texture;
                        material->SetUniformInt("h_HasAlbedoMap", 1);
                        material->SetUniformInt("u_HasAlbedoMap", 1);
                        material->SetUniformInt("h_HasOpacityMap", 1);
                        material->SetUniformInt("u_HasOpacityMap", 1);
                    } else {
                        SPD_WARNING("Light gizmo icon texture load failed for UUID: " << iconUuid);
                    }
                }

                s_LightGizmoMaterials[i] = std::move(material);
            }
        }

        void RenderLightGizmosForView(
            RenderViewHandle handle,
            const RenderView& view,
            const Mat4& camProj,
            const Mat4& camView,
            const Vec3& camPos,
            IStateCache* stateCache,
            RenderViewHandle sceneViewHandle)
        {
            if (handle != sceneViewHandle) return;
            if (s_LightGizmoQueue.empty() || !s_LightGizmoMesh || !stateCache) return;

            const float viewportHeight = static_cast<float>(view.framebuffer ? view.framebuffer->GetHeight() : GraphicsManager::GetScreenHeight());
            if (viewportHeight <= 0.0f) return;

            Vec3 camRight{
                camView.GetElement(0, 0),
                camView.GetElement(0, 1),
                camView.GetElement(0, 2)
            };
            Vec3 camUp{
                camView.GetElement(1, 0),
                camView.GetElement(1, 1),
                camView.GetElement(1, 2)
            };

            if (camRight.LengthSquared() < 1e-6f) camRight = { 1.0f, 0.0f, 0.0f };
            if (camUp.LengthSquared() < 1e-6f) camUp = { 0.0f, 1.0f, 0.0f };
            camRight.Normalize();
            camUp.Normalize();

            std::array<std::vector<InstanceData>, 4> batchedInstances;
            for (auto& batch : batchedInstances) {
                batch.reserve(s_LightGizmoQueue.size());
            }

            for (const auto& command : s_LightGizmoQueue) {
                const auto index = ToLightTypeIndex(command.lightType);

                const float distance = (command.position - camPos).Length();
                const float worldSize = ComputeWorldSizeForPixels(LIGHT_GIZMO_PIXEL_SIZE, distance, camProj, viewportHeight);
                if (!std::isfinite(worldSize) || worldSize <= 0.0f) continue;

                InstanceData instance{};
                instance.model = BuildBillboardMatrix(command.position, camRight, camUp, worldSize);
                instance.idRGB = command.idRGB;
                batchedInstances[index].push_back(instance);
            }

            for (size_t i = 0; i < batchedInstances.size(); ++i) {
                auto& instances = batchedInstances[i];
                if (instances.empty()) continue;

                auto material = s_LightGizmoMaterials[i];
                if (!material || !material->GetPipeline() || !material->GetPipeline()->GetSpecification().shader) continue;

                auto pipeline = material->GetPipeline();
                auto shader = pipeline->GetSpecification().shader;

                stateCache->Bind(pipeline);
                material->SetUniformInt("i_FogEnabled", 0);
                material->Bind();

                shader->SetUniformMat4("u_View", camView);
                shader->SetUniformMat4("u_Projection", camProj);
                shader->SetUniformVec3("u_CameraPos", camPos);
                shader->SetUniformInt("i_FogEnabled", 0);

                OpenGL::GLGeometryBuffer::UpdateInstanceBuffer(
                    instances.data(),
                    instances.size() * sizeof(InstanceData)
                );

                s_LightGizmoMesh->Bind();
                s_LightGizmoMesh->DrawInstanced(instances.size());
                s_LightGizmoMesh->Unbind();
            }
        }
    }

    uint32_t GraphicsManager::s_ScreenWidth = 1920;
    uint32_t GraphicsManager::s_ScreenHeight = 1080;
    std::vector<ECS::Component::Light*> GraphicsManager::m_lights;
    int GraphicsManager::drawCount = 0;
    bool GraphicsManager::enableSorting = true;

    std::unique_ptr<ICommandBuffer> GraphicsManager::s_CommandBuffer;
    std::unique_ptr<Skybox> GraphicsManager::s_skybox;
    EditorCamera* GraphicsManager::s_EditorCamera;
	std::unique_ptr<IStateCache> GraphicsManager::s_StateCache;
	std::unique_ptr<DrawQueue> GraphicsManager::s_DrawQueue;
	std::unique_ptr<RenderViewManager> GraphicsManager::s_RenderViewManager;
    RenderViewHandle GraphicsManager::s_SceneViewHandle;
    RenderViewHandle GraphicsManager::s_GameViewHandle;
    RenderViewHandle GraphicsManager::s_FinalOutputViewHandle;
    RenderViewHandle GraphicsManager::s_FinalGameOutputHandle;
	std::shared_ptr<IClusteredLighting> GraphicsManager::s_clusteredLighting;
    std::unique_ptr<PostProcessPipeline> GraphicsManager::s_PostPipeline;

	std::unique_ptr<ShadowRenderer> GraphicsManager::s_shadowRenderer;

    RenderSettings GraphicsManager::renderSettings;

    PostProcessingSettings GraphicsManager::postProcessingSettings;

    void GraphicsManager::Init() {
        s_CommandBuffer = std::make_unique<OpenGL::GLCommandBuffer>();
        s_skybox = std::make_unique<Skybox>();
        s_StateCache = std::make_unique<OpenGL::GLStateCache>();
        s_DrawQueue = std::make_unique<DrawQueue>();
		s_RenderViewManager = std::make_unique<RenderViewManager>();

        s_shadowRenderer = std::make_unique<ShadowRenderer>();
        s_shadowRenderer->Init();

#ifndef PRODUCTION_BUILD
        s_SceneViewHandle = s_RenderViewManager->CreateHDR(1920, 1080, true);
#endif // !PRODUCTION_BUILD
        s_FinalOutputViewHandle = s_RenderViewManager->Create(1920, 1080, false);
        s_FinalGameOutputHandle = s_RenderViewManager->Create(1920, 1080, false);

        s_clusteredLighting = std::make_shared<OpenGL::GLClusteredLighting>();

        InitDebugPrimitives();
        DebugDrawSystem::SetStateCache(s_StateCache.get());
        NE::Graphics::OpenGL::GLGeometryBuffer::InitInstanceBuffer();
        InitializeLightGizmoResources();


        //// Load Primitives
        //auto skinned = std::make_shared<OpenGL::GLShader>();
        //skinned->LoadFromFile("Library/Shaders/Skinned.nanoshader");
        //skinned->LoadFromFile("Library/Shaders/Skinned.nanoshader");
        //Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLShader>(skinned, "Skinned");

        // initialize UI renderer
        s_ScreenWidth = static_cast<uint32_t>(1920);
        s_ScreenHeight = static_cast<uint32_t>(1080);

        UIRenderer::Init(s_ScreenWidth, s_ScreenHeight, s_RenderViewManager.get());

        s_PostPipeline = std::make_unique<PostProcessPipeline>();
        s_PostPipeline->Init(s_RenderViewManager.get(), s_ScreenWidth, s_ScreenHeight);
        s_PostPipeline->SetSettings(&postProcessingSettings);
    }

    void GraphicsManager::BeginFrame() {
		s_StateCache->InvalidateAll();
        drawCount = 0;
    }

    void GraphicsManager::DrawFrame() {
        NE_PROFILE_FUNCTION();

        for (const auto& [handle, view] : s_RenderViewManager->GetAllRenderViews()) {
            if (!view.isActive) continue;
            if (view.isMain && view.order == 0) s_GameViewHandle = handle;

            const auto& commands = s_DrawQueue->GetCommands();
            s_shadowRenderer->Update(view, m_lights, commands);

            s_RenderViewManager->Bind(handle);
            s_CommandBuffer->Begin();

			// Invalidate cached state per view
			s_StateCache->InvalidateAll();

            const Mat4& camProj = view.projection;
            const Mat4& camView = view.view;
            const Vec3& camPos = view.position;

            // Sort by RenderQueue -> Material -> Mesh
            if (enableSorting)
                s_DrawQueue->Sort(camPos);

            static const int MAX_SHADOWS = 16;
            std::vector<Math::Mat4> shadowVPs;
            std::vector<GLuint>     shadowTextures;
            shadowVPs.reserve(MAX_SHADOWS);
            shadowTextures.reserve(MAX_SHADOWS);
            ECS::Component::Light* dirForSplits = nullptr;

            int shadowCount = 0;
            for (auto* l : m_lights) {
                if (!l) continue;
                l->shadowIndex = -1;

                if (l->shadowType == NE::ECS::Component::Light::None) continue;

                // Directional CSM
                if (l->type == NE::ECS::Component::Light::Directional && 
                    l->shadowCascadeCount == NE::ECS::Component::Light::DIR_CASCADES) 
                {
                    if (shadowCount + NE::ECS::Component::Light::DIR_CASCADES > MAX_SHADOWS) continue;

                    if (!dirForSplits) dirForSplits = l;

                    l->shadowIndex = shadowCount;
                    for (int c = 0; c < NE::ECS::Component::Light::DIR_CASCADES; ++c) {
                        shadowVPs.push_back(l->dirLightVP[c]);
                        shadowTextures.push_back(l->dirShadowTex[c]);
                        ++shadowCount;
                    }
                    continue;
                }

                // Single-map (Spot)
                if (l->shadowMapTex != 0 && l->shadowCascadeCount == 1) {
                    if (shadowCount >= MAX_SHADOWS) continue;
                    l->shadowIndex = shadowCount;
                    shadowVPs.push_back(l->lightViewProj);
                    shadowTextures.push_back(l->shadowMapTex);
                    ++shadowCount;
                }
            }

            s_clusteredLighting->BuildForView(view, m_lights);

            // Prepare instance data buffer and batching variables
            std::vector<InstanceData> instanceData;
            instanceData.reserve(32);
            std::shared_ptr<IGeometryBuffer> currentMesh;
            std::shared_ptr<Material> currentMaterial;
            bool currentReceiveShadows = false;

            auto flushBatch = [&]() {
                if (instanceData.empty() || !currentMesh || !currentMaterial || !currentMaterial->GetPipeline()->GetSpecification().shader)
                    return;

                NE::Graphics::OpenGL::GLGeometryBuffer::UpdateInstanceBuffer(
                    instanceData.data(),
                    instanceData.size() * sizeof(InstanceData)
                );

                // Bind pipeline & GL state
                auto pipeline = currentMaterial->GetPipeline();
                s_StateCache->Bind(pipeline);
                currentMaterial->Bind();
                currentMesh->Bind();

                // Upload transform matrix to shader
                auto shader = pipeline->GetSpecification().shader;
                shader->SetUniformMat4("u_View", camView);
                shader->SetUniformMat4("u_Projection", camProj);
                shader->SetUniformVec3("u_CameraPos", camPos);

                shader->SetUniformVec3("i_GlobalAmbientColor", renderSettings.ambientColour);
                shader->SetUniformFloat("i_GlobalAmbientIntensity", renderSettings.ambientIntensity);

                shader->SetUniformInt("i_FogEnabled", renderSettings.fogEnabled ? 1 : 0);
                shader->SetUniformVec3("i_FogColor", renderSettings.fogColour);
                shader->SetUniformInt("i_FogMode", static_cast<int>(renderSettings.fogMode));
                shader->SetUniformFloat("i_FogDensity", renderSettings.fogDensity);
                shader->SetUniformFloat("i_FogStart", renderSettings.fogStart);
                shader->SetUniformFloat("i_FogEnd", renderSettings.fogEnd);


                int numShadows = static_cast<int>(shadowVPs.size());
                if (numShadows > 16) numShadows = 16;

                shader->SetUniformInt("i_NumShadowMaps", numShadows);
                shader->SetUniformInt("i_ReceiveShadows", currentReceiveShadows ? 1 : 0);

                int dirCascadeCount = 0;
                float dirSplits[NE::ECS::Component::Light::DIR_CASCADES] = {};

                if (dirForSplits) {
                    dirCascadeCount = dirForSplits->shadowCascadeCount;
                    for (int c = 0; c < NE::ECS::Component::Light::DIR_CASCADES; ++c)
                        dirSplits[c] = dirForSplits->dirCascadeSplitsVS[c];
                }

                shader->SetUniformInt("i_DirCascadeCount", ECS::Component::Light::DIR_CASCADES);

                for (int c = 0; c < NE::ECS::Component::Light::DIR_CASCADES; ++c) {
                    std::string splitName = "i_DirCascadeSplitsVS[" + std::to_string(c) + "]";
                    shader->SetUniformFloat(splitName.c_str(), dirSplits[c]);
                }

                for (int i = 0; i < numShadows; ++i) {
                    std::string vpName = "i_ShadowVP[" + std::to_string(i) + "]";
                    std::string texName = "i_ShadowMaps[" + std::to_string(i) + "]";

                    shader->SetUniformMat4(vpName.c_str(), shadowVPs[i]);

                    int unit = 5 + i; // reserve slots 5..(5+numShadows-1)
                    shader->SetUniformInt(texName.c_str(), unit);
                    glActiveTexture(GL_TEXTURE0 + unit);
                    glBindTexture(GL_TEXTURE_2D, shadowTextures[i]);
                }

                // Set lights
                s_clusteredLighting->BindForDraw();

                // Draw mesh with instancing
                currentMesh->DrawInstanced(instanceData.size());
                currentMesh->Unbind();

                instanceData.clear();

                if (view.isMain && view.order == 0)
                    ++drawCount;
                };

            const Frustum frustum = Frustum::ExtractPlanesFromVP(camProj * camView);
            for (const auto& command : commands) {
                if (!frustum.IntersectsSphere(command.boundsCenterWS, command.boundsRadiusWs))
                    continue;

                auto mesh = command.mesh;
                auto material = command.material;
                bool receives = command.receivesShadow;

                // Check compatibility with current batch
                bool compatible =
                    (mesh == currentMesh) &&
                    (material == currentMaterial) &&
                    (receives == currentReceiveShadows);

                // Flush current batch if not compatible
                if (!compatible && !instanceData.empty()) {
                    flushBatch();
                }

                // Prepare to create new batch if not compatible
                if (!compatible) {
                    currentMesh = mesh;
                    currentMaterial = material;
                    currentReceiveShadows = receives;
                }

                NE::Graphics::InstanceData instance{};
                instance.model = command.transform;
                instance.idRGB = command.idRGB;

                instanceData.push_back(instance);
            }

            if (!instanceData.empty()) {
                flushBatch();
            }

            if (s_skybox) {
                s_StateCache->Bind(s_skybox->GetSkyboxPipeline());
                s_skybox->Draw(view);
            }

            RenderLightGizmosForView(handle, view, camProj, camView, camPos, s_StateCache.get(), s_SceneViewHandle);

            if (handle == 1) // Assuming editor camera handle will always be 1
                DrawAllDebugGeometry();

            s_RenderViewManager->Unbind();
        }

        s_StateCache->Reset();

        if (s_PostPipeline) {
            // Scene View
            Math::Mat4 invProj;
            if (s_EditorCamera) {
                invProj = s_EditorCamera->GetProjectionMatrix().Inverse();
            } else {
                invProj.SetToIdentity();
            }
            s_PostPipeline->Execute(s_SceneViewHandle, s_FinalOutputViewHandle, invProj, true);

            // Game View
            auto& views = s_RenderViewManager->GetAllRenderViews();
            auto it = views.find(s_GameViewHandle);
            if (it != views.end()) {
                Math::Mat4 gameInvProj = it->second.projection.Inverse();
                s_PostPipeline->Execute(s_GameViewHandle, s_FinalGameOutputHandle, gameInvProj, false);
            }
        }
    }

    void GraphicsManager::Submit(const DrawCommand& command) {
		s_DrawQueue->Submit(command);
    }

    void GraphicsManager::SubmitLightGizmo(const LightGizmoCommand& command) {
        s_LightGizmoQueue.push_back(command);
    }

    void GraphicsManager::EndFrame() {
		s_RenderViewManager->Unbind();
    }

    void GraphicsManager::Clear() {
        s_DrawQueue->Clear();
        s_LightGizmoQueue.clear();
	}

    RenderGraph* GraphicsManager::GetRenderGraph() {
        return s_PostPipeline ? s_PostPipeline->GetRenderGraph() : nullptr;
    }

    TexturePool* GraphicsManager::GetTexturePool() {
        return s_PostPipeline ? s_PostPipeline->GetTexturePool() : nullptr;
    }

    void GraphicsManager::Shutdown()
    {
        if (s_PostPipeline) {
            s_PostPipeline->Shutdown();
            s_PostPipeline.reset();
        }
		s_RenderViewManager->Shutdown();
        s_skybox.reset();
        s_CommandBuffer.reset();
        DebugDrawSystem::Shutdown();

        UIRenderer::Shutdown();
        NE::Graphics::GizmosRenderer::Cleanup();
        NE::Graphics::OpenGL::GLGeometryBuffer::ShutdownInstanceBuffer();

        s_LightGizmoQueue.clear();
        s_LightGizmoMesh.reset();
        for (auto& material : s_LightGizmoMaterials) {
            material.reset();
        }
    }

    void GraphicsManager::SetEditorCamera(EditorCamera* cam) {
        s_EditorCamera = cam;
        DebugDrawSystem::SetEditorCamera(cam);
    }

    EditorCamera* GraphicsManager::GetEditorCamera() {
        return s_EditorCamera;
    }

    void GraphicsManager::UpdateEditorCameraData() {
        s_RenderViewManager->SetCameraData(
            s_SceneViewHandle, 
			s_EditorCamera->GetProjectionMatrix(),
			s_EditorCamera->GetViewMatrix(),
			s_EditorCamera->GetPosition(),
			s_EditorCamera->GetNearPlane(),
			s_EditorCamera->GetFarPlane(),
            false,
            0
        );
    }

    RenderViewHandle GraphicsManager::CreateRenderView(uint32_t width, uint32_t height, bool enablePicking) 
    {
        return s_RenderViewManager->CreateHDR(width, height, enablePicking);
	}

    void GraphicsManager::DestroyRenderView(RenderViewHandle handle) {
        s_RenderViewManager->Destroy(handle);
    }

    void GraphicsManager::SetCameraData(RenderViewHandle viewHandle, const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, float nearPlane, float farPlane, bool isMain, uint16_t order)
    {
		s_RenderViewManager->SetCameraData(viewHandle, projection, view, position, nearPlane, farPlane, isMain, order);
    }

    void GraphicsManager::EnableCamera(RenderViewHandle viewHandle)
    {
        s_RenderViewManager->EnableCamera(viewHandle);
	}

    void GraphicsManager::DisableCamera(RenderViewHandle viewHandle)
    {
        s_RenderViewManager->DisableCamera(viewHandle);
    }

    uint32_t GraphicsManager::ReadPixel(uint32_t x, uint32_t y) {
		return s_RenderViewManager->GetFramebuffer(s_SceneViewHandle)->ReadPixel(x, y);
    }

    void GraphicsManager::ReadPixelRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint32_t>& outIds) {
        s_RenderViewManager->GetFramebuffer(s_SceneViewHandle)->ReadPixelRect(x, y, width, height, outIds);
    }

    uint32_t GraphicsManager::GetSceneColorAttachment() 
    {
        //if (InputManager::IsKeyDown('4')) return s_FinalColorTex;
        //if (InputManager::IsKeyDown('8')) return s_RenderViewManager->GetFramebuffer(s_SceneViewHandle)->GetColorAttachment();
        //if (InputManager::IsKeyDown('9')) return s_SSAOTex;
        //if (InputManager::IsKeyDown('0')) return s_RenderViewManager->GetFramebuffer(s_GameViewHandle)->GetColorAttachment();
        //if (InputManager::IsKeyDown('P')) return s_RenderViewManager->GetFramebuffer(s_FinalGameOutputHandle)->GetColorAttachment();

        auto framebuffer = s_RenderViewManager->GetFramebuffer(s_FinalOutputViewHandle);
        if (framebuffer) {
            return framebuffer->GetColorAttachment();
        }

        return 0;

		//auto framebuffer = s_RenderViewManager->GetFramebuffer(s_SceneViewHandle);
        //  if (framebuffer) {
        //  return framebuffer->GetColorAttachment();
		//}

		//return 0;
	}

    uint32_t GraphicsManager::GetGameColorAttachment() {
		auto framebuffer = s_RenderViewManager->GetFramebuffer(s_FinalGameOutputHandle);
        if (framebuffer) {
            return framebuffer->GetColorAttachment();
		}
		return 0;
    }

    uint32_t GraphicsManager::GetFinalOutputColorAttachment() {
        auto framebuffer = s_RenderViewManager->GetFramebuffer(s_FinalOutputViewHandle);
        if (framebuffer) {
            return framebuffer->GetColorAttachment();
        }
        return 0;
	}

    void GraphicsManager::DisplayFinalOutput(int windowWidth, int windowHeight)
    {
        //bool hasActiveMainView = false;
        //const auto& views = s_RenderViewManager->GetAllRenderViews();
        //for (const auto& [handle, view] : views) {
        //    if (view.isActive && view.isMain) {
        //        hasActiveMainView = true;
        //        break;
        //    }
        //}

        //if (hasActiveMainView) {
            s_RenderViewManager->BlitToScreen(s_FinalGameOutputHandle, windowWidth, windowHeight);
        //} else {
        //    s_RenderViewManager->BlitToScreen(s_FinalOutputViewHandle, windowWidth, windowHeight);
        //}
    }

    IStateCache* GraphicsManager::GetStateCache() {
        return s_StateCache.get();
    }

    uint32_t GraphicsManager::GetScreenWidth() {
        return s_ScreenWidth;
    }

    uint32_t GraphicsManager::GetScreenHeight() {
        return s_ScreenHeight;
    }

    void GraphicsManager::InitDebugPrimitives() {
        DebugDrawSystem::Init();
    }

    void GraphicsManager::AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color) 
    {
        DebugDrawSystem::AddLine(from, to, color);
    }

    void GraphicsManager::DrawDebugLines() 
    {
        DebugDrawSystem::DrawLines();
    }

    void GraphicsManager::AddDebugTriangle(const Math::Vec3& v0, const Math::Vec3& v1, const Math::Vec3& v2, const Math::Vec3& color) {
        DebugDrawSystem::AddTriangle(v0, v1, v2, color);
    }

    void GraphicsManager::DrawDebugTriangles() {
        DebugDrawSystem::DrawTriangles();
    }

    void GraphicsManager::AddDebugLinesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color) {
        DebugDrawSystem::AddLinesBatch(positions, color);
    }

    void GraphicsManager::AddDebugTrianglesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color) {
        DebugDrawSystem::AddTrianglesBatch(positions, color);
    }

    void GraphicsManager::DrawAllDebugGeometry() {
        DebugDrawSystem::DrawAll();
    }

    void GraphicsManager::DrawUI() {
        UIRenderer::BeginFrame();
        UIRenderer::DrawUIFrame();
        //UIRenderer::DrawTestQuad();
        UIRenderer::EndFrame();
        UIRenderer::Draw3DUIFrame(s_FinalOutputViewHandle);
        
        UIRenderer::Composite(s_FinalOutputViewHandle);
        UIRenderer::Draw3DUIFrame(s_FinalGameOutputHandle);
        UIRenderer::Composite(s_FinalGameOutputHandle);
        UIRenderer::ClearCommands();
    }
}
