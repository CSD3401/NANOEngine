#include "pch.h"
#include "GraphicsManager.hpp"
#include "PostProcessPipeline.hpp"

#include "EditorCamera.hpp"
#include "Skybox.hpp"
#include "Material.hpp"
#include "Model.hpp"
#include "DrawCommand.hpp"
#include "DecalCommand.hpp"
#include "DecalGizmoCommand.hpp"
#include "LightGizmoCommand.hpp"
#include "DrawQueue.hpp"
#include "RenderViewManager.hpp"
#include "RenderSettings.hpp"
#include "ECS/Components/DecalProjector.hpp"
#include "ECS/Components/Transform.hpp"

#include "Math/Mat4.hpp"
#include "Math/Vec3.hpp"

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
#include <glad/glad.h>
#include "glfw/glfw3.h"
#include "Core/SpdLogger.hpp"
#include "InstanceData.hpp"
#include "../OpenGL/GLGeometryBuffer.hpp"
#include "../OpenGL/GLFrameBuffer.hpp"
#include "../OpenGL/GLClusteredLighting.hpp"
#include "../OpenGL/GLVertexBuffer.hpp"
#include "../OpenGL/GLIndexBuffer.hpp"

#include "GizmosRenderer.hpp"
#include "Graphics/DebugRenderer/DebugDrawSystem.hpp"
#include "Core/Profiler.hpp"
#include "Engine.hpp"
#include "ECS/Components/Light.hpp"
#include "SceneManagement/Scene.hpp"
#include "SceneManagement/SceneLightmapRuntime.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "ECS/Core/Entity.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <variant>
#include <string>
#include <unordered_map>

#include "Input/InputManager.hpp"
#include "ShadowRenderer.hpp"
#include "Frustum.hpp"


namespace NE::Graphics {
    namespace {
        constexpr float ICON_GIZMO_PIXEL_SIZE = 128.f;
        constexpr uint32_t TAA_HALTON_PERIOD = 8;

        constexpr std::array<const char*, 4> LIGHT_GIZMO_ICON_UUIDS = {
            "nedirlight", // Directional
            "nepointlight", // Point
            "nespotlight", // Spot
            "nedirlight"  // Area
        };

        std::vector<LightGizmoCommand> s_LightGizmoQueue;
        std::vector<DecalGizmoCommand> s_DecalGizmoQueue;
        std::shared_ptr<IGeometryBuffer> s_LightGizmoMesh;
        std::array<std::shared_ptr<Material>, 4> s_LightGizmoMaterials;
        std::shared_ptr<Material> s_DecalGizmoMaterial;
        std::shared_ptr<IGeometryBuffer> s_DecalCubeMesh;
        GLuint s_LtcMatTexture = 0;
        GLuint s_LtcAmpTexture = 0;
        std::uint64_t s_LtcMatHandle = 0;
        std::uint64_t s_LtcAmpHandle = 0;

        inline Math::Vec3 TransformPoint(const Math::Mat4& M, const Math::Vec3& p) {
            Math::Vec4 v = M * Math::Vec4(p.x, p.y, p.z, 1.0f);
            return { v.x, v.y, v.z };
        }

        inline InstanceData BuildInstanceData(const DrawCommand& command) {
            InstanceData instance{};
            instance.model = command.transform;
            instance.idRGB = command.idRGB;
            instance.lightmapEnabled = command.lightmapEnabled ? 1.0f : 0.0f;
            instance.lightmapUvScale = command.lightmapUvScale;
            instance.lightmapUvOffset = command.lightmapUvOffset;
            instance.lightmapPageSlot = command.lightmapPageSlot;
            return instance;
        }

        void BindSceneLightmapUniforms(IShader& shader) {
            const auto* runtimeState = SceneManagement::GetSceneLightmapRuntimeState(&NE::GetScene());
            if (!runtimeState || !runtimeState->lightingUsable || runtimeState->irradianceHandles.empty()) {
                shader.SetUniformInt("u_LightmapPageCount", 0);
                return;
            }

            const int pageCount = static_cast<int>(std::min<std::size_t>(
                runtimeState->irradianceHandles.size(),
                static_cast<std::size_t>(SceneManagement::kMaxSceneLightmapPages)));
            shader.SetUniformInt("u_LightmapPageCount", pageCount);
            shader.SetUniformHandlev("u_LightmapPages", runtimeState->irradianceHandles.data(), pageCount);
        }

        inline size_t ToLightTypeIndex(ECS::Component::Light::Type type) {
            const uint8_t raw = static_cast<uint8_t>(type);
            return (raw < s_LightGizmoMaterials.size()) ? static_cast<size_t>(raw) : static_cast<size_t>(0);
        }

        inline bool IsPerspectiveProjection(const Mat4& projection) {
            return std::abs(projection.GetElement(3, 3)) < 0.5f;
        }

        inline float Halton(uint32_t index, uint32_t base) {
            float f = 1.0f;
            float r = 0.0f;
            uint32_t i = index;
            while (i > 0) {
                f /= static_cast<float>(base);
                r += f * static_cast<float>(i % base);
                i /= base;
            }
            return r;
        }

        inline std::array<float, 2> ComputeTAAJitterNDC(uint64_t frameIndex, RenderViewHandle handle, float width, float height) {
            if (width <= 0.0f || height <= 0.0f) {
                return { 0.0f, 0.0f };
            }

            const uint32_t sequenceIndex = static_cast<uint32_t>((frameIndex + static_cast<uint64_t>(handle)) % TAA_HALTON_PERIOD) + 1;
            const float haltonX = Halton(sequenceIndex, 2);
            const float haltonY = Halton(sequenceIndex, 3);

            const float jitterX = (haltonX - 0.5f) * 2.0f / width;
            const float jitterY = (haltonY - 0.5f) * 2.0f / height;
            return { jitterX, jitterY };
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

        constexpr int LIGHT_DEBUG_CIRCLE_SEGMENTS = 48;
        constexpr int LIGHT_DEBUG_CONE_RAYS = 8;
        constexpr float LIGHT_DEBUG_MIN_RANGE = 0.05f;
        constexpr float LIGHT_DEBUG_MIN_ANGLE_DEG = 0.1f;
        constexpr float LIGHT_DEBUG_MAX_ANGLE_DEG = 89.0f;

        inline Vec3 BuildPerpendicular(const Vec3& direction) {
            Vec3 reference = (std::abs(direction.y) < 0.99f) ? Vec3{ 0.0f, 1.0f, 0.0f } : Vec3{ 1.0f, 0.0f, 0.0f };
            Vec3 perpendicular = direction.Cross(reference);
            if (perpendicular.LengthSquared() < 1e-6f) {
                reference = { 0.0f, 0.0f, 1.0f };
                perpendicular = direction.Cross(reference);
            }
            if (perpendicular.LengthSquared() < 1e-6f) {
                return { 1.0f, 0.0f, 0.0f };
            }
            perpendicular.Normalize();
            return perpendicular;
        }

        inline void AppendWireCircle(
            std::vector<Vec3>& vertices,
            const Vec3& center,
            const Vec3& axisX,
            const Vec3& axisY,
            float radius,
            int segments)
        {
            if (radius <= 0.0f || segments < 3) return;

            Vec3 prev = center + axisX * radius;
            for (int i = 1; i <= segments; ++i) {
                const float angle = (2.0f * Math::PI * static_cast<float>(i)) / static_cast<float>(segments);
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                const Vec3 curr = center + (axisX * c + axisY * s) * radius;
                vertices.push_back(prev);
                vertices.push_back(curr);
                prev = curr;
            }
        }

        inline void AppendWireCone(
            std::vector<Vec3>& vertices,
            const Vec3& apex,
            const Vec3& direction,
            float range,
            float angleDeg,
            int segments,
            int rayCount)
        {
            const float clampedRange = std::max(range, LIGHT_DEBUG_MIN_RANGE);
            const float clampedAngleDeg = std::clamp(angleDeg, LIGHT_DEBUG_MIN_ANGLE_DEG, LIGHT_DEBUG_MAX_ANGLE_DEG);
            const float angleRad = clampedAngleDeg * (Math::PI / 180.0f);

            Vec3 forward = direction;
            if (forward.LengthSquared() < 1e-6f) {
                forward = { 0.0f, -1.0f, 0.0f };
            }
            forward.Normalize();

            Vec3 right = BuildPerpendicular(forward);
            Vec3 up = right.Cross(forward).Normalized();

            const float radius = std::tan(angleRad) * clampedRange;
            const Vec3 baseCenter = apex + forward * clampedRange;

            AppendWireCircle(vertices, baseCenter, right, up, radius, segments);

            const int step = std::max(1, segments / std::max(1, rayCount));
            for (int i = 0; i < segments; i += step) {
                const float angle = (2.0f * Math::PI * static_cast<float>(i)) / static_cast<float>(segments);
                const float c = std::cos(angle);
                const float s = std::sin(angle);
                const Vec3 rimPoint = baseCenter + (right * c + up * s) * radius;
                vertices.push_back(apex);
                vertices.push_back(rimPoint);
            }
        }

        inline void AppendWireRectangle(
            std::vector<Vec3>& vertices,
            const Vec3& center,
            const Vec3& axisX,
            const Vec3& axisY,
            float halfWidth,
            float halfHeight)
        {
            if (halfWidth <= 0.0f || halfHeight <= 0.0f) return;

            const Vec3 corner0 = center - axisX * halfWidth - axisY * halfHeight;
            const Vec3 corner1 = center + axisX * halfWidth - axisY * halfHeight;
            const Vec3 corner2 = center + axisX * halfWidth + axisY * halfHeight;
            const Vec3 corner3 = center - axisX * halfWidth + axisY * halfHeight;

            vertices.push_back(corner0);
            vertices.push_back(corner1);
            vertices.push_back(corner1);
            vertices.push_back(corner2);
            vertices.push_back(corner2);
            vertices.push_back(corner3);
            vertices.push_back(corner3);
            vertices.push_back(corner0);
        }

        //inline void QueueLightDebugGeometryForView(
        //    RenderViewHandle handle,
        //    RenderViewHandle sceneViewHandle,
        //    const std::vector<ECS::Component::Light*>& lights)
        //{
        //    if (handle != sceneViewHandle || lights.empty()) return;

        //    std::vector<Vec3> vertices;
        //    vertices.reserve(256);

        //    for (const ECS::Component::Light* light : lights) {
        //        if (!light) continue;


        //    }
        //}

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

        void InitializeDecalGizmoResources() {
            if (!s_LightGizmoMesh) {
                auto quadModel = Resource::ResourceManager::GetInstance().LoadResource<Model>("builtin:model/quad");
                if (quadModel && !quadModel->meshes.empty()) {
                    s_LightGizmoMesh = quadModel->meshes[0].buffer;
                } else {
                    SPD_WARNING("Decal gizmo mesh initialization failed: builtin quad model not available.");
                    return;
                }
            }

            auto baseMaterial = Resource::ResourceManager::GetInstance().LoadResource<Material>("neunlitmat");
            if (!baseMaterial) {
                SPD_WARNING("Decal gizmo material initialization failed: neunlitmat not available.");
                return;
            }

            auto material = std::make_shared<Material>(*baseMaterial);
            if (!material->GetPipeline()) {
                SPD_WARNING("Decal gizmo material initialization failed: invalid pipeline.");
                return;
            }

            auto spec = material->GetPipeline()->GetSpecification();
            spec.EnableBlending = true;
            spec.EnableDepthTest = true;
            spec.DepthWrite = false;
            spec.CullMode = GL_NONE;
            spec.PolygonMode = GL_FILL;
            material->ApplyPipelineSpec(spec);

            material->SetQueueBase(RenderQueue::OVERLAY);
            material->SetQueueOffset(1);
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

            auto texture = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLTexture>("nedecal");

            texture->MakeResident();
            material->m_Textures["u_AlbedoMap"] = texture;
            material->m_Textures["u_OpacityMap"] = texture;
            material->SetUniformInt("h_HasAlbedoMap", 1);
            material->SetUniformInt("u_HasAlbedoMap", 1);
            material->SetUniformInt("h_HasOpacityMap", 1);
            material->SetUniformInt("u_HasOpacityMap", 1);

            s_DecalGizmoMaterial = std::move(material);
        }

        void CreateRectLightLtcTextures() {
            if (s_LtcMatTexture != 0 && s_LtcAmpTexture != 0) {
                return;
            }

            constexpr int kLtcLutSize = 64;
            std::vector<float> matPixels(static_cast<size_t>(kLtcLutSize) * static_cast<size_t>(kLtcLutSize) * 4u, 0.0f);
            std::vector<float> ampPixels(static_cast<size_t>(kLtcLutSize) * static_cast<size_t>(kLtcLutSize) * 4u, 0.0f);

            for (int y = 0; y < kLtcLutSize; ++y) {
                const float roughness = static_cast<float>(y) / static_cast<float>(kLtcLutSize - 1);
                const float alpha = std::max(roughness * roughness, 0.02f);

                for (int x = 0; x < kLtcLutSize; ++x) {
                    const float ndotv = static_cast<float>(x) / static_cast<float>(kLtcLutSize - 1);
                    const float grazing = 1.0f - ndotv;
                    const float sharpness = 1.0f - alpha;

                    const float scaleX = std::max(0.14f, 1.0f - sharpness * (0.82f + 0.12f * grazing));
                    const float scaleY = std::max(0.10f, 1.0f - sharpness * (0.90f - 0.20f * ndotv));
                    const float skew = grazing * sharpness * 0.35f;
                    const float amplitude = std::max(0.08f, (0.35f + 0.65f * ndotv) * (0.55f + 0.45f * (1.0f - 0.5f * roughness)));

                    const size_t index = (static_cast<size_t>(y) * static_cast<size_t>(kLtcLutSize) + static_cast<size_t>(x)) * 4u;
                    matPixels[index + 0u] = scaleX;
                    matPixels[index + 1u] = skew;
                    matPixels[index + 2u] = scaleY;
                    matPixels[index + 3u] = 1.0f;

                    ampPixels[index + 0u] = amplitude;
                    ampPixels[index + 1u] = 1.0f;
                    ampPixels[index + 2u] = 0.0f;
                    ampPixels[index + 3u] = 1.0f;
                }
            }

            auto createTexture = [](GLuint& texture, const std::vector<float>& pixels) {
                glCreateTextures(GL_TEXTURE_2D, 1, &texture);
                glTextureStorage2D(texture, 1, GL_RGBA16F, kLtcLutSize, kLtcLutSize);
                glTextureSubImage2D(
                    texture,
                    0,
                    0,
                    0,
                    kLtcLutSize,
                    kLtcLutSize,
                    GL_RGBA,
                    GL_FLOAT,
                    pixels.data());
                glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            };

            createTexture(s_LtcMatTexture, matPixels);
            createTexture(s_LtcAmpTexture, ampPixels);

            s_LtcMatHandle = OpenGL::GetClampBindlessHandleForTexture(s_LtcMatTexture);
            s_LtcAmpHandle = OpenGL::GetClampBindlessHandleForTexture(s_LtcAmpTexture);
        }

        void DestroyRectLightLtcTextures() {
            if (s_LtcMatHandle != 0u) {
                glMakeTextureHandleNonResidentARB(s_LtcMatHandle);
                s_LtcMatHandle = 0u;
            }
            if (s_LtcAmpHandle != 0u) {
                glMakeTextureHandleNonResidentARB(s_LtcAmpHandle);
                s_LtcAmpHandle = 0u;
            }
            if (s_LtcMatTexture != 0u) {
                glDeleteTextures(1, &s_LtcMatTexture);
                s_LtcMatTexture = 0u;
            }
            if (s_LtcAmpTexture != 0u) {
                glDeleteTextures(1, &s_LtcAmpTexture);
                s_LtcAmpTexture = 0u;
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
                const float worldSize = ComputeWorldSizeForPixels(ICON_GIZMO_PIXEL_SIZE, distance, camProj, viewportHeight);
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

        void RenderDecalGizmosForView(
            RenderViewHandle handle,
            const RenderView& view,
            const Mat4& camProj,
            const Mat4& camView,
            const Vec3& camPos,
            IStateCache* stateCache,
            RenderViewHandle sceneViewHandle)
        {
            if (handle != sceneViewHandle) return;
            if (s_DecalGizmoQueue.empty() || !s_LightGizmoMesh || !s_DecalGizmoMaterial || !stateCache) return;
            if (!s_DecalGizmoMaterial->GetPipeline() || !s_DecalGizmoMaterial->GetPipeline()->GetSpecification().shader) return;

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

            std::vector<InstanceData> instances;
            instances.reserve(s_DecalGizmoQueue.size());
            for (const auto& command : s_DecalGizmoQueue) {
                const float distance = (command.position - camPos).Length();
                const float worldSize = ComputeWorldSizeForPixels(ICON_GIZMO_PIXEL_SIZE, distance, camProj, viewportHeight);
                if (!std::isfinite(worldSize) || worldSize <= 0.0f) continue;

                InstanceData instance{};
                instance.model = BuildBillboardMatrix(command.position, camRight, camUp, worldSize);
                instance.idRGB = command.idRGB;
                instances.push_back(instance);
            }
            if (instances.empty()) return;

            auto pipeline = s_DecalGizmoMaterial->GetPipeline();
            auto shader = pipeline->GetSpecification().shader;
            stateCache->Bind(pipeline);
            s_DecalGizmoMaterial->Bind();
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

        bool RenderNormalPrepassForView(
            const RenderView& view,
            const std::vector<DrawCommand>& commands,
            const Frustum& frustum,
            const Mat4& camProj,
            const Mat4& camView,
            const std::shared_ptr<OpenGL::GLShader>& normalPrepassShader,
            IStateCache* stateCache)
        {
            if (!view.framebuffer || !view.framebuffer->HasMiniGBuffer() || !normalPrepassShader) {
                return false;
            }

            // Prepass must not inherit decal/gizmo state from previous draws.
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);

            const GLenum gbufferAttachments[2] = { GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers(2, gbufferAttachments);
            glClear(GL_DEPTH_BUFFER_BIT);
            const float normalClear[4] = { 0.5f, 0.5f, 1.0f, 1.0f };
            glClearBufferfv(GL_COLOR, 0, normalClear);
            const float roughnessClear[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
            glClearBufferfv(GL_COLOR, 1, roughnessClear);

            normalPrepassShader->Bind();
            normalPrepassShader->SetUniformMat4("u_View", camView);
            normalPrepassShader->SetUniformMat4("u_Projection", camProj);

            std::vector<InstanceData> instanceData;
            instanceData.reserve(64);
            std::shared_ptr<IGeometryBuffer> currentMesh;
            std::shared_ptr<Material> currentMaterial;

            auto flushBatch = [&]() {
                if (instanceData.empty() || !currentMesh) return;

                float roughness = 1.0f;
                float opacity = 1.0f;
                int hasRoughnessMap = 0;
                int hasOpacityMap = 0;
                int alphaClip = 0;
                float alphaCutoff = 0.5f;

                if (currentMaterial) {
                    auto& floatUniforms = currentMaterial->GetFloatUniforms();
                    auto getFloat = [&floatUniforms](const char* key, float defaultValue) {
                        auto it = floatUniforms.find(key);
                        return it != floatUniforms.end() ? it->second : defaultValue;
                    };

                    roughness = getFloat("u_Roughness", roughness);
                    opacity = getFloat("u_Opacity", opacity);
                    alphaCutoff = getFloat("u_AlphaCutoff", alphaCutoff);

                    const auto& intUniforms = currentMaterial->m_IntUniforms;
                    auto getInt = [&intUniforms](const char* key, int defaultValue) {
                        auto it = intUniforms.find(key);
                        return it != intUniforms.end() ? it->second : defaultValue;
                    };

                    hasRoughnessMap = getInt("h_HasRoughnessMap", 0);
                    hasOpacityMap = getInt("h_HasOpacityMap", 0);
                    alphaClip = getInt("u_AlphaClip", 0);

                    const auto& textures = currentMaterial->GetTextures();
                    auto bindTextureIfPresent = [&](const char* name, int unit) -> bool {
                        auto texIt = textures.find(name);
                        if (texIt == textures.end() || !texIt->second) return false;
                        glActiveTexture(GL_TEXTURE0 + unit);
                        glBindTexture(GL_TEXTURE_2D, texIt->second->GLName());
                        return true;
                    };

                    if (!bindTextureIfPresent("u_RoughnessMap", 0)) {
                        hasRoughnessMap = 0;
                    }
                    if (!bindTextureIfPresent("u_OpacityMap", 1)) {
                        hasOpacityMap = 0;
                    }
                }

                normalPrepassShader->SetUniformFloat("u_Roughness", roughness);
                normalPrepassShader->SetUniformInt("u_RoughnessMap", 0);
                normalPrepassShader->SetUniformInt("h_HasRoughnessMap", hasRoughnessMap);
                normalPrepassShader->SetUniformFloat("u_Opacity", opacity);
                normalPrepassShader->SetUniformInt("u_OpacityMap", 1);
                normalPrepassShader->SetUniformInt("h_HasOpacityMap", hasOpacityMap);
                normalPrepassShader->SetUniformInt("u_AlphaClip", alphaClip);
                normalPrepassShader->SetUniformFloat("u_AlphaCutoff", alphaCutoff);

                OpenGL::GLGeometryBuffer::UpdateInstanceBuffer(
                    instanceData.data(),
                    instanceData.size() * sizeof(InstanceData)
                );

                currentMesh->Bind();
                currentMesh->DrawInstanced(instanceData.size());
                currentMesh->Unbind();

                instanceData.clear();
            };

            for (const auto& command : commands) {
                if (!frustum.IntersectsSphere(command.boundsCenterWS, command.boundsRadiusWs))
                    continue;
                if (!command.mesh) continue;
                if ((command.mesh != currentMesh || command.material != currentMaterial) && !instanceData.empty()) {
                    flushBatch();
                }

                if (command.mesh != currentMesh || command.material != currentMaterial) {
                    currentMesh = command.mesh;
                    currentMaterial = command.material;
                }

                InstanceData instance = BuildInstanceData(command);
                instance.idRGB = { 0.0f, 0.0f, 0.0f };
                instanceData.push_back(instance);
            }

            flushBatch();

            if (stateCache) {
                stateCache->InvalidateAll();
            }

            view.framebuffer->SetPickingWrite(view.framebuffer->HasPickingAttachment());
            return true;
        }

        void RenderDecalsForView(
            const RenderView& view,
            const Mat4& camProj,
            const Mat4& camView,
            const Vec3& camPos,
            const std::vector<DecalCommand>& decals,
            IStateCache* stateCache)
        {
            if (decals.empty() || !stateCache || !s_DecalCubeMesh || !view.framebuffer) return;
            if (!view.framebuffer->HasMiniGBuffer()) return;

            const uint32_t sceneDepth = view.framebuffer->GetDepthAttachment();
            const uint32_t sceneNormal = view.framebuffer->GetNormalAttachment();
            if (sceneDepth == 0 || sceneNormal == 0) return;

            const Mat4 invViewProj = (camProj * camView).Inverse();

            const GLenum colorAttachment = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &colorAttachment);

            const GLboolean wasBlend = glIsEnabled(GL_BLEND);
            const GLboolean wasDepthTest = glIsEnabled(GL_DEPTH_TEST);
            GLint depthMask = GL_TRUE;
            glGetIntegerv(GL_DEPTH_WRITEMASK, &depthMask);

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);

            for (const auto& decal : decals) {
                if (!decal.material || !decal.material->GetPipeline()) continue;

                const float maxDistance = std::max(0.0f, decal.drawDistance);
                const float distanceToCamera = (decal.positionWS - camPos).Length();
                if (maxDistance > 0.0f && distanceToCamera > maxDistance) continue;

                float fade = 1.0f;
                const float fadeStart = std::clamp(decal.startFadeDistance, 0.0f, maxDistance);
                if (maxDistance > fadeStart && distanceToCamera > fadeStart) {
                    fade = 1.0f - ((distanceToCamera - fadeStart) / (maxDistance - fadeStart));
                    fade = std::clamp(fade, 0.0f, 1.0f);
                }
                if (fade <= 0.001f) continue;

                auto pipeline = decal.material->GetPipeline();
                auto shader = pipeline->GetSpecification().shader;
                if (!shader) continue;

                stateCache->Bind(pipeline);
                decal.material->SetUniformFloat("u_DecalDistanceFade", fade);
                decal.material->Bind();

                shader->SetUniformMat4("u_View", camView);
                shader->SetUniformMat4("u_Projection", camProj);
                shader->SetUniformMat4("u_ViewProjInv", invViewProj);
                shader->SetUniformMat4("u_DecalModel", decal.model);
                shader->SetUniformMat4("u_DecalInvModel", decal.invModel);
                shader->SetUniformVec3("u_CameraPos", camPos);

                shader->SetUniformInt("u_SceneDepthTex", 12);
                glActiveTexture(GL_TEXTURE12);
                glBindTexture(GL_TEXTURE_2D, sceneDepth);

                shader->SetUniformInt("u_SceneNormalTex", 13);
                glActiveTexture(GL_TEXTURE13);
                glBindTexture(GL_TEXTURE_2D, sceneNormal);

                s_DecalCubeMesh->Bind();
                s_DecalCubeMesh->Draw();
                s_DecalCubeMesh->Unbind();
            }

            if (!wasBlend) glDisable(GL_BLEND);
            if (wasDepthTest) glEnable(GL_DEPTH_TEST);
            glDepthMask(depthMask == GL_TRUE ? GL_TRUE : GL_FALSE);

            view.framebuffer->SetPickingWrite(view.framebuffer->HasPickingAttachment());
        }

#ifndef PRODUCTION_BUILD
        std::shared_ptr<OpenGL::GLShader> CreateEditorDebugViewShader()
        {
            constexpr const char* kVertexSource = R"(
            #version 460 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec3 aNormal;
            layout(location = 2) in vec2 aUV0;
            layout(location = 4) in vec2 aUV1;

            uniform mat4 u_Model;
            uniform mat4 u_View;
            uniform mat4 u_Projection;

            out vec3 vNormalWS;
            out vec2 vUV0;
            out vec2 vUV1;

            void main() {
                vec4 worldPos = u_Model * vec4(aPos, 1.0);
                mat3 normalMtx = transpose(inverse(mat3(u_Model)));
                vNormalWS = normalize(normalMtx * aNormal);
                vUV0 = aUV0;
                vUV1 = aUV1;
                gl_Position = u_Projection * u_View * worldPos;
            }
            )";

                        constexpr const char* kFragmentSource = R"(
            #version 460 core
            #extension GL_ARB_bindless_texture : require

            in vec3 vNormalWS;
            in vec2 vUV0;
            in vec2 vUV1;

            layout(location = 0) out vec4 FragColor;

            const int MAX_LIGHTMAP_PAGES = 128;

            uniform int u_PreviewMode;
            uniform float u_UvScale;
            uniform sampler2D u_LightmapPages[MAX_LIGHTMAP_PAGES];
            uniform int u_LightmapPageCount;
            uniform int u_LightmapEnabled;
            uniform vec2 u_LightmapUvScale;
            uniform vec2 u_LightmapUvOffset;
            uniform int u_LightmapPageSlot;

            void main() {
                vec3 outColor = vec3(0.0);
                if (u_PreviewMode == 1) {
                    outColor = normalize(vNormalWS) * 0.5 + 0.5;
                } else if (u_PreviewMode == 2) {
                    vec2 uv = fract(vUV0 * max(u_UvScale, 0.0001));
                    outColor = vec3(uv, 0.0);
                } else if (u_PreviewMode == 3) {
                    vec2 uv = fract(vUV1 * max(u_UvScale, 0.0001));
                    outColor = vec3(uv, 0.0);
                } else if (u_PreviewMode == 4) {
                    if (u_LightmapEnabled != 0) {
                        vec2 atlasUv = vUV1 * u_LightmapUvScale + u_LightmapUvOffset;
                        vec2 debugUv = fract(atlasUv * max(u_UvScale, 0.0001));
                        outColor = vec3(debugUv, 0.0);
                    } else {
                        outColor = vec3(0.2, 0.0, 0.2);
                    }
                } else if (u_PreviewMode == 5) {
                    if (u_LightmapEnabled != 0 && u_LightmapPageSlot >= 0 && u_LightmapPageSlot < u_LightmapPageCount) {
                        vec2 atlasUv = vUV1 * u_LightmapUvScale + u_LightmapUvOffset;
                        vec3 baked = texture(u_LightmapPages[u_LightmapPageSlot], atlasUv).rgb;
                        outColor = baked / (vec3(1.0) + max(baked, vec3(0.0)));
                    } else {
                        outColor = vec3(0.0);
                    }
                }
                FragColor = vec4(outColor, 1.0);
            }
            )";

            auto compileStage = [](GLenum stage, const char* source) -> GLuint {
                GLuint shader = glCreateShader(stage);
                if (!shader) return 0;
                glShaderSource(shader, 1, &source, nullptr);
                glCompileShader(shader);

                GLint ok = GL_FALSE;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
                if (ok == GL_TRUE) {
                    return shader;
                }

                GLint len = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
                std::string log;
                if (len > 1) {
                    log.resize(static_cast<size_t>(len));
                    glGetShaderInfoLog(shader, len, nullptr, log.data());
                }
                SPD_WARNING("EditorDebugView shader compile failed: " << log);
                glDeleteShader(shader);
                return 0;
            };

            GLuint vs = compileStage(GL_VERTEX_SHADER, kVertexSource);
            GLuint fs = compileStage(GL_FRAGMENT_SHADER, kFragmentSource);
            if (!vs || !fs) {
                if (vs) glDeleteShader(vs);
                if (fs) glDeleteShader(fs);
                return nullptr;
            }

            GLuint program = glCreateProgram();
            glAttachShader(program, vs);
            glAttachShader(program, fs);
            glLinkProgram(program);

            glDetachShader(program, vs);
            glDetachShader(program, fs);
            glDeleteShader(vs);
            glDeleteShader(fs);

            GLint linked = GL_FALSE;
            glGetProgramiv(program, GL_LINK_STATUS, &linked);
            if (linked != GL_TRUE) {
                GLint len = 0;
                glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
                std::string log;
                if (len > 1) {
                    log.resize(static_cast<size_t>(len));
                    glGetProgramInfoLog(program, len, nullptr, log.data());
                }
                SPD_WARNING("EditorDebugView shader link failed: " << log);
                glDeleteProgram(program);
                return nullptr;
            }

            return std::make_shared<OpenGL::GLShader>(program);
        }

        bool RenderEditorDebugViewPassForView(
            RenderViewHandle handle,
            RenderViewHandle sceneViewHandle,
            const RenderView& view,
            const std::vector<DrawCommand>& commands,
            const Frustum& frustum,
            const Mat4& camProj,
            const Mat4& camView,
            const std::shared_ptr<OpenGL::GLShader>& debugShader,
            GraphicsManager::ScenePreviewMode previewMode,
            float uvScale,
            IStateCache* stateCache)
        {
            if (handle != sceneViewHandle || !view.framebuffer || !debugShader) {
                return false;
            }

            if (previewMode == GraphicsManager::ScenePreviewMode::Shaded) {
                return false;
            }

            const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
            const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
            GLboolean depthMaskWasEnabled = GL_TRUE;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);
            GLint previousDepthFunc = GL_LESS;
            glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

            view.framebuffer->SetPickingWrite(false);

            const GLenum colorAttachment = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &colorAttachment);

            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);

            const float clearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
            glClearBufferfv(GL_COLOR, 0, clearColor);

            debugShader->Bind();
            debugShader->SetUniformMat4("u_View", camView);
            debugShader->SetUniformMat4("u_Projection", camProj);
            debugShader->SetUniformInt("u_PreviewMode", static_cast<int>(previewMode));
            debugShader->SetUniformFloat("u_UvScale", std::max(0.0001f, uvScale));
            BindSceneLightmapUniforms(*debugShader);

            for (const auto& command : commands) {
                if (!command.mesh) continue;
                if (command.material && command.material->GetQueueBase() == RenderQueue::OVERLAY) continue;
                if (!frustum.IntersectsSphere(command.boundsCenterWS, command.boundsRadiusWs)) continue;

                debugShader->SetUniformMat4("u_Model", command.transform);
                debugShader->SetUniformInt("u_LightmapEnabled", command.lightmapEnabled ? 1 : 0);
                debugShader->SetUniformVec2("u_LightmapUvScale", command.lightmapUvScale);
                debugShader->SetUniformVec2("u_LightmapUvOffset", command.lightmapUvOffset);
                debugShader->SetUniformInt(
                    "u_LightmapPageSlot",
                    command.lightmapPageSlot == std::numeric_limits<std::uint32_t>::max()
                    ? -1
                    : static_cast<int>(command.lightmapPageSlot));

                command.mesh->Bind();
                if (command.hasUv1) {
                    glEnableVertexAttribArray(4);
                } else {
                    glDisableVertexAttribArray(4);
                    glVertexAttrib2f(4, 0.0f, 0.0f);
                }

                command.mesh->Draw();

                if (!command.hasUv1) {
                    glEnableVertexAttribArray(4);
                }
                command.mesh->Unbind();
            }

            if (stateCache) {
                stateCache->InvalidateAll();
            }

            if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            glDepthFunc(previousDepthFunc);
            glDepthMask(depthMaskWasEnabled);

            view.framebuffer->SetPickingWrite(view.framebuffer->HasPickingAttachment());
            return true;
        }
#endif
    }

    uint32_t GraphicsManager::s_ScreenWidth = 1920;
    uint32_t GraphicsManager::s_ScreenHeight = 1080;
	uint32_t GraphicsManager::s_GameViewWidth = 1920;
	uint32_t GraphicsManager::s_GameViewHeight = 1080;
    std::vector<RenderLightRef> GraphicsManager::m_lights;
    int GraphicsManager::drawCount = 0;
    bool GraphicsManager::enableSorting = true;

    std::unique_ptr<ICommandBuffer> GraphicsManager::s_CommandBuffer;
    std::unique_ptr<Skybox> GraphicsManager::s_skybox;
    EditorCamera* GraphicsManager::s_EditorCamera;
	std::unique_ptr<IStateCache> GraphicsManager::s_StateCache;
	std::unique_ptr<DrawQueue> GraphicsManager::s_DrawQueue;
    std::vector<DecalCommand> GraphicsManager::s_DecalQueue;
	std::unique_ptr<RenderViewManager> GraphicsManager::s_RenderViewManager;
    RenderViewHandle GraphicsManager::s_SceneViewHandle;
    RenderViewHandle GraphicsManager::s_GameViewHandle;
    RenderViewHandle GraphicsManager::s_FinalOutputViewHandle;
    RenderViewHandle GraphicsManager::s_FinalGameOutputHandle;
    std::shared_ptr<IClusteredLighting> GraphicsManager::s_clusteredLighting;
    std::unique_ptr<PostProcessPipeline> GraphicsManager::s_PostPipeline;
    std::shared_ptr<OpenGL::GLShader> GraphicsManager::s_NormalPrepassShader;
#ifndef PRODUCTION_BUILD
    std::shared_ptr<OpenGL::GLShader> GraphicsManager::s_EditorDebugViewShader;
#endif
    std::unordered_set<uint32_t> GraphicsManager::s_SelectedEntityIds;
    GraphicsManager::ScenePreviewMode GraphicsManager::s_ScenePreviewMode = GraphicsManager::ScenePreviewMode::Shaded;
    float GraphicsManager::s_ScenePreviewUvScale = 1.0f;

	std::shared_ptr<IGeometryBuffer> GraphicsManager::s_particleQuadMesh;

	std::unique_ptr<ShadowRenderer> GraphicsManager::s_shadowRenderer;

    RenderSettings GraphicsManager::renderSettings;

    PostProcessingSettings GraphicsManager::postProcessingSettings;
    SelectionHighlightSettings GraphicsManager::selectionHighlightSettings;

    void GraphicsManager::Init() {
        s_CommandBuffer = std::make_unique<OpenGL::GLCommandBuffer>();
        s_skybox = std::make_unique<Skybox>();
        s_StateCache = std::make_unique<OpenGL::GLStateCache>();
        s_DrawQueue = std::make_unique<DrawQueue>();
		s_RenderViewManager = std::make_unique<RenderViewManager>();

        s_shadowRenderer = std::make_unique<ShadowRenderer>();
        s_shadowRenderer->Init();

#ifndef PRODUCTION_BUILD
        {
            RenderViewCreateDesc desc;
            desc.width = 1920;
            desc.height = 1080;
            desc.enablePicking = true;
            desc.enableMiniGBuffer = true;
            desc.enableDepth = true;
            desc.enableStencil = false;
            desc.format = RenderViewFormat::HDR;
            s_SceneViewHandle = s_RenderViewManager->Create(desc);
        }
#endif // !PRODUCTION_BUILD
        {
            RenderViewCreateDesc desc;
            desc.width = 1920;
            desc.height = 1080;
            desc.enablePicking = false;
            desc.enableMiniGBuffer = false;
            desc.enableDepth = false;
            desc.enableStencil = false;
            desc.format = RenderViewFormat::Standard;
            s_FinalOutputViewHandle = s_RenderViewManager->Create(desc);
            s_FinalGameOutputHandle = s_RenderViewManager->Create(desc);
        }

        s_clusteredLighting = std::make_shared<OpenGL::GLClusteredLighting>();
        CreateRectLightLtcTextures();

        InitDebugPrimitives();
        DebugDrawSystem::SetStateCache(s_StateCache.get());
        NE::Graphics::OpenGL::GLGeometryBuffer::InitInstanceBuffer();
        InitializeLightGizmoResources();
        InitializeDecalGizmoResources();
        s_NormalPrepassShader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("nenormalprepass");
#ifndef PRODUCTION_BUILD
        s_EditorDebugViewShader = CreateEditorDebugViewShader();
        if (!s_EditorDebugViewShader) {
            SPD_WARNING("Editor debug view shader failed to load.");
        }
#endif

        auto decalCubeModel = Resource::ResourceManager::GetInstance().LoadResource<Model>("builtin:model/cube");
        if (decalCubeModel && !decalCubeModel->meshes.empty()) {
            s_DecalCubeMesh = decalCubeModel->meshes[0].buffer;
        } else {
            SPD_WARNING("Decal cube mesh initialization failed: builtin cube model not available.");
        }

        s_ScreenWidth = static_cast<uint32_t>(1920);
        s_ScreenHeight = static_cast<uint32_t>(1080);
		s_GameViewWidth = s_ScreenWidth;
		s_GameViewHeight = s_ScreenHeight;

        s_PostPipeline = std::make_unique<PostProcessPipeline>();
        s_PostPipeline->Init(s_RenderViewManager.get(), s_GameViewWidth, s_GameViewHeight);
        s_PostPipeline->SetSettings(&postProcessingSettings);
        s_PostPipeline->SetSelectionSettings(&selectionHighlightSettings);
#ifndef PRODUCTION_BUILD
		s_PostPipeline->SetSelectedEntityIds(&s_SelectedEntityIds);
#endif
    }

    void GraphicsManager::BeginFrame() {
		s_StateCache->InvalidateAll();
        drawCount = 0;
    }

    void GraphicsManager::DrawFrame() {
#ifndef PRODUCTION_BUILD
        NE_PROFILE_FUNCTION();
#endif // !PRODUCTION_BUILD

        static uint64_t s_TAAFrameIndex = 0;
        const auto& allViews = s_RenderViewManager->GetAllRenderViews();
        const auto orderedViewHandles = s_RenderViewManager->GetOrderedActiveViews();
        std::unordered_map<RenderViewHandle, Mat4> frameViewMatrices;
        std::unordered_map<RenderViewHandle, Mat4> frameProjMatrices;

        s_GameViewHandle = InvalidRenderView;
        for (const RenderViewHandle handle : orderedViewHandles) {
            auto it = allViews.find(handle);
            if (it == allViews.end()) continue;
            const auto& view = it->second;
            if (view.isMain && view.order == 0) {
                s_GameViewHandle = handle;
                break;
            }
        }

		// Keep the main game view framebuffer in sync with the Game panel resolution.
		if (s_GameViewHandle != InvalidRenderView && s_RenderViewManager) {
			auto fb = s_RenderViewManager->GetFramebuffer(s_GameViewHandle);
			if (fb && (fb->GetWidth() != s_GameViewWidth || fb->GetHeight() != s_GameViewHeight)) {
				s_RenderViewManager->Resize(s_GameViewHandle, s_GameViewWidth, s_GameViewHeight);
			}
		}

        for (const RenderViewHandle handle : orderedViewHandles) {
            auto it = allViews.find(handle);
            if (it == allViews.end()) continue;
            const auto& view = it->second;

            const auto& commands = s_DrawQueue->GetCommands();
            // Shadow fitting must use the unjittered camera matrices from RenderView.
            s_shadowRenderer->Update(view, m_lights, commands);

            s_RenderViewManager->Bind(handle);
            s_CommandBuffer->Begin();

			// Invalidate cached state per view
			s_StateCache->InvalidateAll();
			if (view.framebuffer) {
				view.framebuffer->SetPickingWrite(view.framebuffer->HasPickingAttachment());
				if (view.framebuffer->HasPickingAttachment()) {
					// Prevent stale IDs when generating selection masks from the picking buffer.
					const float clearId[4] = { 0, 0, 0, 0 };
					glClearBufferfv(GL_COLOR, 1, clearId);
				}
			}

            Mat4 camProj = view.projection;
            const Mat4& camView = view.view;
            const Vec3& camPos = view.position;

            if (postProcessingSettings.taaSettings.enabled && IsPerspectiveProjection(camProj)) {
                const float width = view.framebuffer ? static_cast<float>(view.framebuffer->GetWidth()) : static_cast<float>(s_ScreenWidth);
                const float height = view.framebuffer ? static_cast<float>(view.framebuffer->GetHeight()) : static_cast<float>(s_ScreenHeight);
                const auto jitterNdc = ComputeTAAJitterNDC(s_TAAFrameIndex, handle, width, height);
                camProj.GetElement(0, 2) += jitterNdc[0];
                camProj.GetElement(1, 2) += jitterNdc[1];
            }

            frameViewMatrices[handle] = camView;
            frameProjMatrices[handle] = camProj;
            const Frustum frustum = Frustum::ExtractPlanesFromVP(camProj * camView);

            const bool ranPrepass = RenderNormalPrepassForView(
                view,
                commands,
                frustum,
                camProj,
                camView,
                s_NormalPrepassShader,
                s_StateCache.get()
            );
            if (ranPrepass) {
                glDepthFunc(GL_LEQUAL);
            }

            // Sort by RenderQueue -> Material -> Mesh
            if (enableSorting)
                s_DrawQueue->Sort(camPos);

            static const int MAX_SHADOWS = 16;
            std::vector<Math::Mat4> shadowVPs;
            std::vector<GLuint>     shadowTextures;
            shadowVPs.reserve(MAX_SHADOWS);
            shadowTextures.reserve(MAX_SHADOWS);
            RenderLightRef* dirForSplits = nullptr;

            int shadowCount = 0;
            for (auto& lightRef : m_lights) {
                auto* l = lightRef.light;
                if (!l) continue;

                LightShadowRuntime* runtime = s_shadowRenderer ? s_shadowRenderer->FindRuntime(lightRef.entity) : nullptr;
                if (runtime) {
                    runtime->shadowIndex = -1;
                }

                if (l->shadowType == NE::ECS::Component::Light::None ||
                    l->shadowUpdateMode != NE::ECS::Component::Light::Realtime ||
                    l->type == NE::ECS::Component::Light::Area) continue;
                if (!runtime) continue;

                // Directional CSM
                if (l->type == NE::ECS::Component::Light::Directional && 
                    runtime->shadowCascadeCount == NE::ECS::Component::Light::DIR_CASCADES) 
                {
                    if (shadowCount + NE::ECS::Component::Light::DIR_CASCADES > MAX_SHADOWS) continue;

                    if (!dirForSplits) dirForSplits = &lightRef;

                    runtime->shadowIndex = shadowCount;
                    for (int c = 0; c < NE::ECS::Component::Light::DIR_CASCADES; ++c) {
                        shadowVPs.push_back(runtime->dirLightVP[c]);
                        shadowTextures.push_back(runtime->dirShadowTex[c]);
                        ++shadowCount;
                    }
                    continue;
                }

                // Single-map (Spot)
                if (runtime->shadowMapTex != 0 && runtime->shadowCascadeCount == 1) {
                    if (shadowCount >= MAX_SHADOWS) continue;
                    runtime->shadowIndex = shadowCount;
                    shadowVPs.push_back(runtime->lightViewProj);
                    shadowTextures.push_back(runtime->shadowMapTex);
                    ++shadowCount;
                }
            }

            RenderView viewForLighting = view;
            viewForLighting.projection = camProj;
            s_clusteredLighting->BuildForView(viewForLighting, *s_shadowRenderer, m_lights);

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
                BindSceneLightmapUniforms(*shader);
                shader->SetUniformHandle("u_LtcMatTexture", s_LtcMatHandle);
                shader->SetUniformHandle("u_LtcAmpTexture", s_LtcAmpHandle);

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
                    const LightShadowRuntime* dirRuntime = s_shadowRenderer ? s_shadowRenderer->FindRuntime(dirForSplits->entity) : nullptr;
                    // Directional cascade splits are derived from the active view, so one upload
                    // is shared across directional lights in the current renderer design.
                    if (dirRuntime) {
                        dirCascadeCount = dirRuntime->shadowCascadeCount;
                        for (int c = 0; c < NE::ECS::Component::Light::DIR_CASCADES; ++c)
                            dirSplits[c] = dirRuntime->dirCascadeSplitsVS[c];
                    }
                }

                shader->SetUniformInt("i_DirCascadeCount", dirCascadeCount);

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

            for (const auto& command : commands) {
                // Skip OVERLAY commands during view rendering - they'll be rendered separately after post-processing
                if (command.material && command.material->GetQueueBase() == RenderQueue::OVERLAY) {
                    continue;
                }

                if (!frustum.IntersectsSphere(command.boundsCenterWS, command.boundsRadiusWs)) {
                    continue;
                }

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

                instanceData.push_back(BuildInstanceData(command));
            }

            if (!instanceData.empty()) {
                flushBatch();
            }

            

            RenderDecalsForView(view, camProj, camView, camPos, s_DecalQueue, s_StateCache.get());

            if (s_skybox) {
                s_StateCache->Bind(s_skybox->GetSkyboxPipeline());
                RenderView skyboxView = view;
                skyboxView.projection = camProj;
                s_skybox->Draw(skyboxView);
            }

            RenderParticlesForView(view, camProj, camView, camPos, frustum, drawCount);

#ifndef PRODUCTION_BUILD
            RenderEditorDebugViewPassForView(
                handle,
                s_SceneViewHandle,
                view,
                commands,
                frustum,
                camProj,
                camView,
                s_EditorDebugViewShader,
                s_ScenePreviewMode,
                s_ScenePreviewUvScale,
                s_StateCache.get()
            );
            RenderLightGizmosForView(handle, view, camProj, camView, camPos, s_StateCache.get(), s_SceneViewHandle);
            RenderDecalGizmosForView(handle, view, camProj, camView, camPos, s_StateCache.get(), s_SceneViewHandle);
#endif

            if (handle == s_SceneViewHandle) {
				if (view.framebuffer) {
					// Debug rendering doesn't write an entity ID output; prevent it from corrupting the picking buffer.
					view.framebuffer->SetPickingWrite(false);
				}
				DrawAllDebugGeometry();
			}

            s_RenderViewManager->Unbind();
            if (ranPrepass) {
                glDepthFunc(GL_LESS);
            }
        }

        s_StateCache->Reset();

        if (s_PostPipeline) {
#ifndef PRODUCTION_BUILD
            // Scene View
            const auto sceneSourceFramebuffer = s_RenderViewManager->GetFramebuffer(s_SceneViewHandle);
            const auto sceneDestFramebuffer = s_RenderViewManager->GetFramebuffer(s_FinalOutputViewHandle);
            if (sceneSourceFramebuffer && sceneDestFramebuffer) {
                Math::Mat4 sceneView;
                Math::Mat4 sceneProj;
                auto viewIt = frameViewMatrices.find(s_SceneViewHandle);
                auto projIt = frameProjMatrices.find(s_SceneViewHandle);
                if (viewIt != frameViewMatrices.end()) {
                    sceneView = viewIt->second;
                } else if (s_EditorCamera) {
                    sceneView = s_EditorCamera->GetViewMatrix();
                } else {
                    sceneView.SetToIdentity();
                }

                if (projIt != frameProjMatrices.end()) {
                    sceneProj = projIt->second;
                } else if (s_EditorCamera) {
                    sceneProj = s_EditorCamera->GetProjectionMatrix();
                } else {
                    sceneProj.SetToIdentity();
                }

                Math::Mat4 invProj = sceneProj.Inverse();
                s_PostPipeline->Execute(
                    s_SceneViewHandle,
                    s_FinalOutputViewHandle,
                    invProj,
                    sceneView,
                    sceneProj,
                    true
                );
            }
#endif

            // Game View
            auto it = allViews.find(s_GameViewHandle);
            const auto gameSourceFramebuffer = s_RenderViewManager->GetFramebuffer(s_GameViewHandle);
            const auto gameDestFramebuffer = s_RenderViewManager->GetFramebuffer(s_FinalGameOutputHandle);
            if (it != allViews.end() && gameSourceFramebuffer && gameDestFramebuffer) {
                Math::Mat4 gameView = it->second.view;
                Math::Mat4 gameProj = it->second.projection;

                auto gameViewIt = frameViewMatrices.find(s_GameViewHandle);
                auto gameProjIt = frameProjMatrices.find(s_GameViewHandle);
                if (gameViewIt != frameViewMatrices.end()) {
                    gameView = gameViewIt->second;
                }
                if (gameProjIt != frameProjMatrices.end()) {
                    gameProj = gameProjIt->second;
                }

                Math::Mat4 gameInvProj = gameProj.Inverse();
                s_PostPipeline->Execute(s_GameViewHandle, s_FinalGameOutputHandle, gameInvProj, gameView, gameProj, false);
            }
        }

        // Render UI (OVERLAY queue) after post-processing to final output framebuffers (Not sure if should be below or above post-processing? For now, render after so that UI is always crisp and unaffected by TAA)
        RenderUIOverlay();
        ++s_TAAFrameIndex;
    }

    uint32_t GraphicsManager::DecodeEntityIdFromRGB(const Math::Vec3& idRGB) {
        if (idRGB.x < 0.0f || idRGB.y < 0.0f || idRGB.z < 0.0f) {
            return ECS::NO_ENTITY;
        }

        auto channelToByte = [](float channel) -> uint32_t {
            const float scaled = std::round(channel * 255.0f);
            return static_cast<uint32_t>(std::clamp(scaled, 0.0f, 255.0f));
        };

        const uint32_t r = channelToByte(idRGB.x);
        const uint32_t g = channelToByte(idRGB.y);
        const uint32_t b = channelToByte(idRGB.z);
        return r | (g << 8) | (b << 16);
    }

    bool GraphicsManager::IsSelectedDrawCommand(const DrawCommand& command) {
        if (!command.mesh) return false;
        const uint32_t entityId = DecodeEntityIdFromRGB(command.idRGB);
        if (entityId == ECS::NO_ENTITY) return false;
        return s_SelectedEntityIds.find(entityId) != s_SelectedEntityIds.end();
    }

#if 0 // Legacy geometry-based selection mask (removed). Selection highlight now derives from picking.
    void GraphicsManager::EnsureSelectionMaskResources(const RenderView& view)
    {
        if (!view.framebuffer) return;

        const uint32_t width = view.framebuffer->GetWidth();
        const uint32_t height = view.framebuffer->GetHeight();
        if (width == 0 || height == 0) return;
        if (s_SelectionMaskTexture != 0 && s_SelectionMaskFBO != 0 &&
            s_SelectionMaskWidth == width && s_SelectionMaskHeight == height) {
            return;
        }

        ReleaseSelectionMaskResources();

        glGenTextures(1, &s_SelectionMaskTexture);
        glBindTexture(GL_TEXTURE_2D, s_SelectionMaskTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenFramebuffers(1, &s_SelectionMaskFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, s_SelectionMaskFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_SelectionMaskTexture, 0);
        const GLenum attachment = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &attachment);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            SPD_ERROR("Selection mask framebuffer is incomplete.");
            ReleaseSelectionMaskResources();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        s_SelectionMaskWidth = width;
        s_SelectionMaskHeight = height;
    }

    void GraphicsManager::ReleaseSelectionMaskResources()
    {
        if (s_SelectionMaskFBO != 0) {
            glDeleteFramebuffers(1, &s_SelectionMaskFBO);
            s_SelectionMaskFBO = 0;
        }
        if (s_SelectionMaskTexture != 0) {
            glDeleteTextures(1, &s_SelectionMaskTexture);
            s_SelectionMaskTexture = 0;
        }
        s_SelectionMaskWidth = 0;
        s_SelectionMaskHeight = 0;
    }

    void GraphicsManager::RenderSelectionMaskForView(
        RenderViewHandle handle,
        const RenderView& view,
        const Frustum& frustum,
        const Math::Mat4& camProj,
        const Math::Mat4& camView,
        const std::vector<DrawCommand>& commands)
    {
        if (handle != s_SceneViewHandle) return;
        if (!selectionHighlightSettings.enabled) return;
        if (s_SelectedEntityIds.empty()) return;
        if (!view.framebuffer || !view.framebuffer->HasDepth()) return;
        if (view.framebuffer->GetDepthAttachment() == 0) return;
        if (!s_SelectionMaskShader) return;

        EnsureSelectionMaskResources(view);
        if (s_SelectionMaskTexture == 0 || s_SelectionMaskFBO == 0) return;

        const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean depthWriteWasEnabled = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteWasEnabled);
        GLint depthFunc = GL_LESS;
        GLint cullFaceMode = GL_BACK;
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        if (cullWasEnabled) {
            glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
        }

        const GLenum attachment = GL_COLOR_ATTACHMENT0;
        glBindFramebuffer(GL_FRAMEBUFFER, s_SelectionMaskFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_SelectionMaskTexture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, view.framebuffer->GetDepthAttachment(), 0);
        glDrawBuffers(1, &attachment);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            SPD_ERROR("Selection mask pass framebuffer is incomplete.");
            glBindFramebuffer(GL_FRAMEBUFFER, view.framebuffer->GetFramebuffer());
            return;
        }

        glViewport(0, 0, static_cast<GLsizei>(view.framebuffer->GetWidth()), static_cast<GLsizei>(view.framebuffer->GetHeight()));
        const GLuint clearMask[4] = { 0u, 0u, 0u, 0u };
        glClearBufferuiv(GL_COLOR, 0, clearMask);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        // This pass re-renders visible selected geometry against the populated scene depth buffer,
        // so equal-depth fragments must pass to reproduce the visible footprint reliably.
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        s_SelectionMaskShader->Bind();
        s_SelectionMaskShader->SetUniformMat4("u_View", camView);
        s_SelectionMaskShader->SetUniformMat4("u_Projection", camProj);

        std::vector<InstanceData> instanceData;
        instanceData.reserve(64);
        std::shared_ptr<IGeometryBuffer> currentMesh;
        std::shared_ptr<Material> currentMaterial;

        auto flushBatch = [&]() {
            if (instanceData.empty() || !currentMesh) return;

            float opacity = 1.0f;
            int hasOpacityMap = 0;
            int alphaClip = 0;
            float alphaCutoff = 0.5f;
            Math::Vec3 tiling{ 1.0f, 1.0f, 1.0f };
            Math::Vec3 offset{ 0.0f, 0.0f, 0.0f };
            GLenum cullMode = GL_BACK;
            bool enableCull = true;

            if (currentMaterial) {
                const auto& floatUniforms = currentMaterial->GetFloatUniforms();
                auto getFloat = [&floatUniforms](const char* key, float defaultValue) {
                    auto it = floatUniforms.find(key);
                    return it != floatUniforms.end() ? it->second : defaultValue;
                };

                opacity = getFloat("u_Opacity", opacity);
                alphaCutoff = getFloat("u_AlphaCutoff", alphaCutoff);

                const auto& intUniforms = currentMaterial->m_IntUniforms;
                auto getInt = [&intUniforms](const char* key, int defaultValue) {
                    auto it = intUniforms.find(key);
                    return it != intUniforms.end() ? it->second : defaultValue;
                };

                hasOpacityMap = getInt("h_HasOpacityMap", 0);
                alphaClip = getInt("u_AlphaClip", 0);

                const auto& vec3Uniforms = currentMaterial->GetVec3Uniforms();
                auto getVec3 = [&vec3Uniforms](const char* key, const Math::Vec3& defaultValue) {
                    auto it = vec3Uniforms.find(key);
                    return it != vec3Uniforms.end() ? it->second : defaultValue;
                };

                tiling = getVec3("u_Tiling", tiling);
                offset = getVec3("u_Offset", offset);

                if (auto pipeline = currentMaterial->GetPipeline()) {
                    cullMode = pipeline->GetSpecification().CullMode;
                    enableCull = (cullMode != GL_NONE);
                }

                const auto& textures = currentMaterial->GetTextures();
                auto opacityTexIt = textures.find("u_OpacityMap");
                if (opacityTexIt != textures.end() && opacityTexIt->second) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, opacityTexIt->second->GLName());
                } else {
                    hasOpacityMap = 0;
                }
            }

            if (enableCull) {
                glEnable(GL_CULL_FACE);
                glCullFace(cullMode);
            } else {
                glDisable(GL_CULL_FACE);
            }

            s_SelectionMaskShader->SetUniformFloat("u_Opacity", opacity);
            s_SelectionMaskShader->SetUniformInt("u_OpacityMap", 0);
            s_SelectionMaskShader->SetUniformInt("h_HasOpacityMap", hasOpacityMap);
            s_SelectionMaskShader->SetUniformInt("u_AlphaClip", alphaClip);
            s_SelectionMaskShader->SetUniformFloat("u_AlphaCutoff", alphaCutoff);
            s_SelectionMaskShader->SetUniformVec3("u_Tiling", tiling);
            s_SelectionMaskShader->SetUniformVec3("u_Offset", offset);

            OpenGL::GLGeometryBuffer::UpdateInstanceBuffer(
                instanceData.data(),
                instanceData.size() * sizeof(InstanceData)
            );

            currentMesh->Bind();
            currentMesh->DrawInstanced(instanceData.size());
            currentMesh->Unbind();
            instanceData.clear();
        };

        for (const auto& command : commands) {
            if (!IsSelectedDrawCommand(command)) continue;
            if (!command.material) continue;
            const RenderQueue queue = command.material->GetQueueBase();
            if (queue != RenderQueue::GEOMETRY && queue != RenderQueue::ALPHATEST) continue;
            if (!frustum.IntersectsSphere(command.boundsCenterWS, command.boundsRadiusWs)) continue;
            if (!command.mesh) continue;

            if ((command.mesh != currentMesh || command.material != currentMaterial) && !instanceData.empty()) {
                flushBatch();
            }

            if (command.mesh != currentMesh || command.material != currentMaterial) {
                currentMesh = command.mesh;
                currentMaterial = command.material;
            }

            InstanceData instance{};
            instance.model = command.transform;
            instance.idRGB = { 0.0f, 0.0f, 0.0f };
            instanceData.push_back(instance);
        }

        flushBatch();

        glBindFramebuffer(GL_FRAMEBUFFER, view.framebuffer->GetFramebuffer());
        view.framebuffer->SetPickingWrite(view.framebuffer->HasPickingAttachment());
        if (s_StateCache) {
            s_StateCache->InvalidateAll();
        }
        glDepthMask(depthWriteWasEnabled);
        glDepthFunc(depthFunc);
        if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cullWasEnabled) {
            glEnable(GL_CULL_FACE);
            glCullFace(cullFaceMode);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
#endif // 0

    void GraphicsManager::Submit(const DrawCommand& command) {
		s_DrawQueue->Submit(command);
    }

    void GraphicsManager::Submit(const ParticleDrawCommand& command) {
		s_DrawQueue->Submit(command);
	}

    void GraphicsManager::SubmitDecal(const DecalCommand& command) {
        s_DecalQueue.push_back(command);
    }

    void GraphicsManager::SubmitDecalGizmo(const DecalGizmoCommand& command) {
        s_DecalGizmoQueue.push_back(command);
    }

    void GraphicsManager::SubmitLightGizmo(const LightGizmoCommand& command) {
        s_LightGizmoQueue.push_back(command);
    }

    void GraphicsManager::EndFrame() {
		s_RenderViewManager->Unbind();
    }

    void GraphicsManager::RenderUIOverlay() {
        const auto& commands = s_DrawQueue->GetCommands();

        // Filter for OVERLAY commands
        std::vector<const DrawCommand*> uiCommands;
        for (const auto& cmd : commands) {
            if (cmd.material && cmd.material->GetQueueBase() == RenderQueue::OVERLAY) {
                uiCommands.push_back(&cmd);
            }
        }

        if (uiCommands.empty()) {
            return;
        }

        // Render UI to both final output views (editor scene view and game view)
        std::vector<RenderViewHandle> outputViews = { s_FinalOutputViewHandle, s_FinalGameOutputHandle };

        for (RenderViewHandle viewHandle : outputViews) {
            // Get the view to check if it exists and is active
            auto& views = s_RenderViewManager->GetAllRenderViews();
            auto it = views.find(viewHandle);
            if (it == views.end() || !it->second.framebuffer) {
                continue;
            }

            // Bind the final output framebuffer
            s_RenderViewManager->Bind(viewHandle);

            // Start with depth disabled (screen-space UI default)
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);

            // Prepare batching
            std::vector<InstanceData> instanceData;
            instanceData.reserve(32);
            std::shared_ptr<IGeometryBuffer> currentMesh;
            std::shared_ptr<Material> currentMaterial;
            std::optional<ScissorRect> currentScissor;
            bool currentDepthTest = false;

            auto flushBatch = [&]() {
                if (instanceData.empty() || !currentMesh || !currentMaterial || !currentMaterial->GetPipeline()->GetSpecification().shader)
                    return;

                // Apply scissor state before drawing this batch
                if (currentScissor.has_value()) {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(currentScissor->x, currentScissor->y,
                              currentScissor->width, currentScissor->height);
                } else {
                    glDisable(GL_SCISSOR_TEST);
                }

                // Apply depth test state for this batch
                if (currentDepthTest) {
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LEQUAL);
                    glDepthMask(GL_FALSE);  // Read depth but don't write (UI doesn't occlude 3D)
                } else {
                    glDisable(GL_DEPTH_TEST);
                    glDepthMask(GL_FALSE);
                }

                NE::Graphics::OpenGL::GLGeometryBuffer::UpdateInstanceBuffer(
                    instanceData.data(),
                    instanceData.size() * sizeof(InstanceData)
                );

                // Bind pipeline & GL state
                auto pipeline = currentMaterial->GetPipeline();
                s_StateCache->Bind(pipeline);
                currentMaterial->Bind();
                currentMesh->Bind();

                // Draw mesh with instancing
                currentMesh->DrawInstanced(instanceData.size());
                currentMesh->Unbind();

                instanceData.clear();
            };

            // Batch and render all UI commands
            for (const DrawCommand* cmdPtr : uiCommands) {
                const DrawCommand& command = *cmdPtr;

                auto mesh = command.mesh;
                auto material = command.material;

                // Check compatibility with current batch (includes scissor rect and depth test)
                bool compatible =
                    (mesh == currentMesh) &&
                    (material == currentMaterial) &&
                    (command.scissorRect == currentScissor) &&
                    (command.enableDepthTest == currentDepthTest);

                // Flush current batch if not compatible
                if (!compatible && !instanceData.empty()) {
                    flushBatch();
                }

                // Prepare to create new batch if not compatible
                if (!compatible) {
                    currentMesh = mesh;
                    currentMaterial = material;
                    currentScissor = command.scissorRect;
                    currentDepthTest = command.enableDepthTest;
                }

                instanceData.push_back(BuildInstanceData(command));
            }

            if (!instanceData.empty()) {
                flushBatch();
            }

            // Restore GL state
            glDisable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);

            // Unbind view after rendering UI
            s_RenderViewManager->Unbind();
        }
    }

    void GraphicsManager::Clear() {
        s_DrawQueue->Clear();
        s_DecalQueue.clear();
        s_DecalGizmoQueue.clear();
        s_LightGizmoQueue.clear();
        DebugDrawSystem::ClearFrameGeometry();
	}

    void GraphicsManager::SetSelectedEntities(const std::vector<uint32_t>& selectedIds) {
        s_SelectedEntityIds.clear();
        s_SelectedEntityIds.reserve(selectedIds.size());
        for (uint32_t id : selectedIds) {
            if (id != ECS::NO_ENTITY) {
                s_SelectedEntityIds.insert(id);
            }
        }
    }

    void GraphicsManager::ClearSelectedEntities() {
        s_SelectedEntityIds.clear();
    }

    RenderGraph* GraphicsManager::GetRenderGraph() {
        return s_PostPipeline ? s_PostPipeline->GetRenderGraph() : nullptr;
    }

    TexturePool* GraphicsManager::GetTexturePool() {
        return s_PostPipeline ? s_PostPipeline->GetTexturePool() : nullptr;
    }

    std::shared_ptr<IGeometryBuffer> GraphicsManager::GetGlobalParticleQuadMesh()
    {
        if (s_particleQuadMesh)
            return s_particleQuadMesh;

        std::vector<Vertex> vertices(4);

        // Positions (centered quad)
        vertices[0].position = { -0.5f, -0.5f, 0.0f };
        vertices[1].position = { 0.5f, -0.5f, 0.0f };
        vertices[2].position = { 0.5f,  0.5f, 0.0f };
        vertices[3].position = { -0.5f,  0.5f, 0.0f };

        // Normals (not important for particles, but your VAO expects it)
        vertices[0].normal = { 0.0f, 0.0f, 1.0f };
        vertices[1].normal = { 0.0f, 0.0f, 1.0f };
        vertices[2].normal = { 0.0f, 0.0f, 1.0f };
        vertices[3].normal = { 0.0f, 0.0f, 1.0f };

        // UVs
        vertices[0].texCoord0 = { 0.0f, 0.0f };
        vertices[1].texCoord0 = { 1.0f, 0.0f };
        vertices[2].texCoord0 = { 1.0f, 1.0f };
        vertices[3].texCoord0 = { 0.0f, 1.0f };

        std::vector<uint32_t> indices = { 0, 1, 2, 0, 2, 3 };

        auto vb = std::make_shared<OpenGL::GLVertexBuffer>(
            vertices.data(),
            static_cast<uint32_t>(vertices.size() * sizeof(Vertex)),
            sizeof(Vertex)
        );

        auto ib = std::make_shared<OpenGL::GLIndexBuffer>(
            indices.data(),
            indices.size()
        );

        auto geo = std::make_shared<OpenGL::GLGeometryBuffer>(vb, ib);

        geo->EnableParticleInstanceLayout(13, 14, 15); // posLS, size, color

        s_particleQuadMesh = geo;
        return s_particleQuadMesh;
    }

    void GraphicsManager::Shutdown()
    {
        if (s_PostPipeline) {
            s_PostPipeline->Shutdown();
            s_PostPipeline.reset();
        }
        if (s_shadowRenderer) {
            s_shadowRenderer->Shutdown();
            s_shadowRenderer.reset();
        }
        DestroyRectLightLtcTextures();
		s_RenderViewManager->Shutdown();
        s_skybox.reset();
        s_CommandBuffer.reset();
        DebugDrawSystem::Shutdown();

        NE::Graphics::GizmosRenderer::Cleanup();
        NE::Graphics::OpenGL::GLGeometryBuffer::ShutdownInstanceBuffer();
		NE::Graphics::OpenGL::GLGeometryBuffer::ShutdownParticleInstanceBuffer();

        s_LightGizmoQueue.clear();
        s_DecalGizmoQueue.clear();
        s_LightGizmoMesh.reset();
        s_DecalGizmoMaterial.reset();
        s_DecalCubeMesh.reset();
        s_DecalQueue.clear();
        m_lights.clear();
        s_NormalPrepassShader.reset();
#ifndef PRODUCTION_BUILD
        s_EditorDebugViewShader.reset();
#endif
        //if (s_SelectionOutlineProgram != 0) {
        //    glDeleteProgram(s_SelectionOutlineProgram);
        //    s_SelectionOutlineProgram = 0;
        //}
        s_SelectedEntityIds.clear();
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
        RenderViewCreateDesc desc;
        desc.width = width;
        desc.height = height;
        desc.enablePicking = enablePicking;
        desc.enableMiniGBuffer = true;
        desc.enableDepth = true;
        desc.enableStencil = false;
        desc.format = RenderViewFormat::HDR;
        return s_RenderViewManager->Create(desc);
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
        auto framebuffer = s_RenderViewManager->GetFramebuffer(s_SceneViewHandle);
        if (!framebuffer) return 0;
		return framebuffer->ReadPixel(x, y);
    }

    void GraphicsManager::ReadPixelRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint32_t>& outIds) {
        auto framebuffer = s_RenderViewManager->GetFramebuffer(s_SceneViewHandle);
        if (!framebuffer) {
            outIds.clear();
            return;
        }

        framebuffer->ReadPixelRect(x, y, width, height, outIds);
    }

    uint32_t GraphicsManager::GetSceneColorAttachment()  {
        auto framebuffer = s_RenderViewManager->GetFramebuffer(s_FinalOutputViewHandle);
        if (framebuffer) {
            return framebuffer->GetColorAttachment();
        }

        return 0;
	}

    uint32_t GraphicsManager::GetSceneDebugAttachment() {
        auto framebuffer = s_RenderViewManager->GetFramebuffer(s_SceneViewHandle);
        if (framebuffer) {
            return framebuffer->GetColorAttachment();
        }
        return 0;
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

	void GraphicsManager::SetGameViewResolution(uint32_t width, uint32_t height) {
		width = std::max(1u, width);
		height = std::max(1u, height);

		if (width == s_GameViewWidth && height == s_GameViewHeight) {
			return;
		}

		s_GameViewWidth = width;
		s_GameViewHeight = height;

		// Resize final game output target (what the Game panel displays) and post FX resources.
		if (s_RenderViewManager && s_FinalGameOutputHandle != InvalidRenderView) {
			s_RenderViewManager->Resize(s_FinalGameOutputHandle, width, height);
		}
		if (s_PostPipeline) {
			s_PostPipeline->Resize(width, height);
		}

		// Avoid ghosting after a resolution change when TAA is enabled.
		postProcessingSettings.taaSettings.resetHistory = true;
	}

	uint32_t GraphicsManager::GetGameViewWidth() {
		return s_GameViewWidth;
	}

	uint32_t GraphicsManager::GetGameViewHeight() {
		return s_GameViewHeight;
	}

	float GraphicsManager::GetGameViewAspect() {
		if (s_GameViewHeight == 0) return 16.0f / 9.0f;
		return static_cast<float>(s_GameViewWidth) / static_cast<float>(s_GameViewHeight);
	}

    void GraphicsManager::SetScenePreviewMode(uint8_t mode) {
#ifndef PRODUCTION_BUILD
        if (mode > static_cast<uint8_t>(ScenePreviewMode::LightmapOnly)) {
            mode = static_cast<uint8_t>(ScenePreviewMode::Shaded);
        }
        s_ScenePreviewMode = static_cast<ScenePreviewMode>(mode);
#else
        (void)mode;
        s_ScenePreviewMode = ScenePreviewMode::Shaded;
#endif
    }

    uint8_t GraphicsManager::GetScenePreviewMode() {
#ifndef PRODUCTION_BUILD
        return static_cast<uint8_t>(s_ScenePreviewMode);
#else
        return static_cast<uint8_t>(ScenePreviewMode::Shaded);
#endif
    }

    void GraphicsManager::SetScenePreviewUvScale(float scale) {
#ifndef PRODUCTION_BUILD
        if (!std::isfinite(scale) || scale <= 0.0f) {
            scale = 1.0f;
        }
        s_ScenePreviewUvScale = scale;
#else
        (void)scale;
        s_ScenePreviewUvScale = 1.0f;
#endif
    }

    float GraphicsManager::GetScenePreviewUvScale() {
#ifndef PRODUCTION_BUILD
        return s_ScenePreviewUvScale;
#else
        return 1.0f;
#endif
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

    void GraphicsManager::DrawSelectedLightGizmos(const ECS::Component::Light& light) {
        std::vector<Vec3> vertices;
        vertices.reserve(256);

        Vec3 baseColor = light.color;
        if (baseColor.LengthSquared() < 1e-6f) {
            baseColor = { 1.0f, 1.0f, 1.0f };
        }
        baseColor.x = std::clamp(baseColor.x, 0.1f, 1.0f);
        baseColor.y = std::clamp(baseColor.y, 0.1f, 1.0f);
        baseColor.z = std::clamp(baseColor.z, 0.1f, 1.0f);

        switch (light.type) {
        case ECS::Component::Light::Type::Point: {
            const auto* pointData = std::get_if<ECS::Component::Light::PointLightData>(&light.data);
            if (!pointData) break;

            const float range = std::max(pointData->range, LIGHT_DEBUG_MIN_RANGE);
            vertices.clear();
            AppendWireCircle(vertices, light.position, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, range, LIGHT_DEBUG_CIRCLE_SEGMENTS);
            AppendWireCircle(vertices, light.position, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, range, LIGHT_DEBUG_CIRCLE_SEGMENTS);
            AppendWireCircle(vertices, light.position, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, range, LIGHT_DEBUG_CIRCLE_SEGMENTS);
            GraphicsManager::AddDebugLinesBatch(vertices, baseColor);
            break;
        }
        case ECS::Component::Light::Type::Spot: {
            const auto* spotData = std::get_if<ECS::Component::Light::SpotLightData>(&light.data);
            if (!spotData) break;

            const float range = std::max(spotData->range, LIGHT_DEBUG_MIN_RANGE);
            vertices.clear();
            AppendWireCone(
                vertices,
                light.position,
                light.direction,
                range,
                spotData->outerConeAngleDeg,
                LIGHT_DEBUG_CIRCLE_SEGMENTS,
                LIGHT_DEBUG_CONE_RAYS
            );
            GraphicsManager::AddDebugLinesBatch(vertices, baseColor);

            if (spotData->innerConeAngleDeg > LIGHT_DEBUG_MIN_ANGLE_DEG) {
                vertices.clear();
                AppendWireCone(
                    vertices,
                    light.position,
                    light.direction,
                    range,
                    std::min(spotData->innerConeAngleDeg, spotData->outerConeAngleDeg),
                    LIGHT_DEBUG_CIRCLE_SEGMENTS,
                    LIGHT_DEBUG_CONE_RAYS
                );
                GraphicsManager::AddDebugLinesBatch(vertices, baseColor * 0.6f);
            }
            break;
        }
        case ECS::Component::Light::Type::Area: {
            const auto* areaData = std::get_if<ECS::Component::Light::AreaLightData>(&light.data);
            if (!areaData) break;

            const float halfWidth = std::max(areaData->width * 0.5f, LIGHT_DEBUG_MIN_RANGE * 0.5f);
            const float halfHeight = std::max(areaData->height * 0.5f, LIGHT_DEBUG_MIN_RANGE * 0.5f);

            Vec3 lightRight = light.right;
            if (lightRight.LengthSquared() < 1e-6f) {
                lightRight = { 1.0f, 0.0f, 0.0f };
            }
            lightRight.Normalize();

            Vec3 lightUp = light.up;
            if (lightUp.LengthSquared() < 1e-6f) {
                lightUp = { 0.0f, 1.0f, 0.0f };
            }
            lightUp.Normalize();

            Vec3 emitNormal = light.direction;
            if (emitNormal.LengthSquared() < 1e-6f) {
                emitNormal = -lightRight.Cross(lightUp);
            }
            if (emitNormal.LengthSquared() < 1e-6f) {
                emitNormal = { 0.0f, 0.0f, -1.0f };
            }
            emitNormal.Normalize();

            vertices.clear();
            AppendWireRectangle(vertices, light.position, lightRight, lightUp, halfWidth, halfHeight);

            const float markerLength = std::max(std::min(std::max(areaData->width, areaData->height) * 0.35f, areaData->range * 0.35f), LIGHT_DEBUG_MIN_RANGE);
            const float markerWing = std::max(markerLength * 0.2f, LIGHT_DEBUG_MIN_RANGE * 0.5f);
            const Vec3 arrowTip = light.position + emitNormal * markerLength;
            const Vec3 arrowBase = arrowTip - emitNormal * markerWing * 1.5f;
            vertices.push_back(light.position);
            vertices.push_back(arrowTip);
            vertices.push_back(arrowTip);
            vertices.push_back(arrowBase + lightRight * markerWing);
            vertices.push_back(arrowTip);
            vertices.push_back(arrowBase - lightRight * markerWing);
            vertices.push_back(arrowTip);
            vertices.push_back(arrowBase + lightUp * markerWing);
            vertices.push_back(arrowTip);
            vertices.push_back(arrowBase - lightUp * markerWing);

            GraphicsManager::AddDebugLinesBatch(vertices, baseColor);
            break;
        }
        default:
            break;
        }
    }

    void GraphicsManager::DrawSelectedDecalGizmos(const ECS::Component::DecalProjector& decal, 
        const ECS::Component::Transform& transform
    ) {
        static constexpr Math::Vec3 kColour = { 0.20f, 0.95f, 1.0f };
        static constexpr Math::Vec3 kCorners[8] = {
            { -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f,  0.5f, -0.5f }, { -0.5f,  0.5f, -0.5f },
            { -0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f,  0.5f }, { 0.5f,  0.5f,  0.5f }, { -0.5f,  0.5f,  0.5f }
        };
        static constexpr int kEdges[12][2] = {
            {0,1}, {1,2}, {2,3}, {3,0},
            {4,5}, {5,6}, {6,7}, {7,4},
            {0,4}, {1,5}, {2,6}, {3,7}
        };

        const Math::Mat4 localPivot = Math::Mat4::BuildTranslation(decal.pivot);
        const Math::Mat4 localScale = Math::Mat4::BuildScaling(decal.width, decal.height, decal.depth);
        const Math::Mat4 projectorModel = transform.worldMatrix * localPivot * localScale;

        std::vector<Math::Vec3> vertices;
        vertices.reserve(24);

        Math::Vec3 wsCorners[8];
        for (int i = 0; i < 8; ++i) {
            wsCorners[i] = TransformPoint(projectorModel, kCorners[i]);
        }

        for (const auto& edge : kEdges) {
            vertices.push_back(wsCorners[edge[0]]);
            vertices.push_back(wsCorners[edge[1]]);
        }

        Graphics::GraphicsManager::AddDebugLinesBatch(vertices, kColour);
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

    void GraphicsManager::RenderParticlesForView(
        const RenderView& view,
        const Math::Mat4& camProj,
        const Math::Mat4& camView,
        const Math::Vec3& camPos,
        const Frustum& frustum,
        int& drawCount)
    {
        const auto& particleCommands = s_DrawQueue->GetParticleCommands();

        for (const auto& pcmd : particleCommands)
        {
            if (!pcmd.material || !pcmd.mesh) continue;
            if (!pcmd.instances || pcmd.instanceCount == 0) continue;

            if (!frustum.IntersectsSphere(pcmd.boundsCenterWS, pcmd.boundsRadiusWS))
                continue;

            auto pipeline = pcmd.material->GetPipeline();
            if (!pipeline || !pipeline->GetSpecification().shader)
                continue;

            s_StateCache->Bind(pipeline);

            if (pcmd.enableDepthTest) glEnable(GL_DEPTH_TEST);
            else glDisable(GL_DEPTH_TEST);

            // Safe transparent-particle state
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            pcmd.material->Bind();
            pcmd.mesh->Bind();

            NE::Graphics::OpenGL::GLGeometryBuffer::UpdateParticleInstanceBuffer(
                pcmd.instances,
                static_cast<size_t>(pcmd.instanceCount) * sizeof(NE::Graphics::ParticleInstanceData)
            );

            auto shader = pipeline->GetSpecification().shader;

            shader->SetUniformMat4("u_View", camView);
            shader->SetUniformMat4("u_Projection", camProj);
            shader->SetUniformVec3("u_CameraPos", camPos);
            shader->SetUniformMat4("u_EmitterModel", pcmd.emitterModel);

            const NE::Math::Mat4 invView = camView.Inverse();
            NE::Math::Vec3 camRightWS = invView.Right(); camRightWS.Normalize();
            NE::Math::Vec3 camUpWS = invView.Up();    camUpWS.Normalize();

            shader->SetUniformVec3("u_CamRightWS", camRightWS);
            shader->SetUniformVec3("u_CamUpWS", camUpWS);

            pcmd.mesh->DrawInstanced(pcmd.instanceCount);
            pcmd.mesh->Unbind();

            // Restore
            glDepthMask(GL_TRUE);

            if (view.isMain && view.order == 0)
                ++drawCount;
        }
    }
}
