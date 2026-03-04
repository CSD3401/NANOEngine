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

#include "GizmosRenderer.hpp"
#include "Graphics/DebugRenderer/DebugDrawSystem.hpp"
#include "Core/Profiler.hpp"
#include "ECS/Components/Light.hpp"
#include "SceneManagement/Scene.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "ECS/Core/Entity.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <variant>
#include <string>
#include <unordered_map>

#include "Input/InputManager.hpp"
#include "ShadowRenderer.hpp"
#include "Frustum.hpp"


namespace NE::Graphics {
    namespace {
        constexpr float SELECTION_OUTLINE_SCALE = 1.05f;
        constexpr Math::Vec4 SELECTION_OUTLINE_COLOR = { 0.89f, 0.61f, 0.06f, 1.0f };

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

        inline Math::Vec3 TransformPoint(const Math::Mat4& M, const Math::Vec3& p) {
            Math::Vec4 v = M * Math::Vec4(p.x, p.y, p.z, 1.0f);
            return { v.x, v.y, v.z };
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

                InstanceData instance{};
                instance.model = command.transform;
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
    std::vector<DecalCommand> GraphicsManager::s_DecalQueue;
	std::unique_ptr<RenderViewManager> GraphicsManager::s_RenderViewManager;
    RenderViewHandle GraphicsManager::s_SceneViewHandle;
    RenderViewHandle GraphicsManager::s_GameViewHandle;
    RenderViewHandle GraphicsManager::s_FinalOutputViewHandle;
    RenderViewHandle GraphicsManager::s_FinalGameOutputHandle;
    std::shared_ptr<IClusteredLighting> GraphicsManager::s_clusteredLighting;
    std::unique_ptr<PostProcessPipeline> GraphicsManager::s_PostPipeline;
    std::shared_ptr<OpenGL::GLShader> GraphicsManager::s_NormalPrepassShader;
    std::shared_ptr<OpenGL::GLShader> GraphicsManager::s_SelectionOutlineProgram;
    std::unordered_set<uint32_t> GraphicsManager::s_SelectedEntityIds;

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
        {
            RenderViewCreateDesc desc;
            desc.width = 1920;
            desc.height = 1080;
            desc.enablePicking = true;
            desc.enableMiniGBuffer = true;
            desc.enableDepth = true;
            desc.enableStencil = true;
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

        InitDebugPrimitives();
        DebugDrawSystem::SetStateCache(s_StateCache.get());
        NE::Graphics::OpenGL::GLGeometryBuffer::InitInstanceBuffer();
        InitializeLightGizmoResources();
        InitializeDecalGizmoResources();
        s_NormalPrepassShader = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("nenormalprepass");
        s_SelectionOutlineProgram = Resource::ResourceManager::GetInstance().LoadResource<OpenGL::GLShader>("neselectionoutline");

        auto decalCubeModel = Resource::ResourceManager::GetInstance().LoadResource<Model>("builtin:model/cube");
        if (decalCubeModel && !decalCubeModel->meshes.empty()) {
            s_DecalCubeMesh = decalCubeModel->meshes[0].buffer;
        } else {
            SPD_WARNING("Decal cube mesh initialization failed: builtin cube model not available.");
        }


        //// Load Primitives
        //auto skinned = std::make_shared<OpenGL::GLShader>();
        //skinned->LoadFromFile("Library/Shaders/Skinned.nanoshader");
        //skinned->LoadFromFile("Library/Shaders/Skinned.nanoshader");
        //Asset::AssetManager::GetInstance().AddToMap<OpenGL::GLShader>(skinned, "Skinned");

        s_ScreenWidth = static_cast<uint32_t>(1920);
        s_ScreenHeight = static_cast<uint32_t>(1080);

        s_PostPipeline = std::make_unique<PostProcessPipeline>();
        s_PostPipeline->Init(s_RenderViewManager.get(), s_ScreenWidth, s_ScreenHeight);
        s_PostPipeline->SetSettings(&postProcessingSettings);
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

        for (const RenderViewHandle handle : orderedViewHandles) {
            auto it = allViews.find(handle);
            if (it == allViews.end()) continue;
            const auto& view = it->second;

            const auto& commands = s_DrawQueue->GetCommands();
            s_shadowRenderer->Update(view, m_lights, commands);

            s_RenderViewManager->Bind(handle);
            s_CommandBuffer->Begin();

			// Invalidate cached state per view
			s_StateCache->InvalidateAll();

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

            RenderView viewForLighting = view;
            viewForLighting.projection = camProj;
            s_clusteredLighting->BuildForView(viewForLighting, m_lights);

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

                NE::Graphics::InstanceData instance{};
                instance.model = command.transform;
                instance.idRGB = command.idRGB;

                instanceData.push_back(instance);
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

            RenderSelectionHighlightForView(handle, view, camProj, camView, commands);

            RenderLightGizmosForView(handle, view, camProj, camView, camPos, s_StateCache.get(), s_SceneViewHandle);
            RenderDecalGizmosForView(handle, view, camProj, camView, camPos, s_StateCache.get(), s_SceneViewHandle);

            if (handle == s_SceneViewHandle)
                DrawAllDebugGeometry();

            s_RenderViewManager->Unbind();
            if (ranPrepass) {
                glDepthFunc(GL_LESS);
            }
        }

        s_StateCache->Reset();

        if (s_PostPipeline) {
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
                s_PostPipeline->Execute(s_SceneViewHandle, s_FinalOutputViewHandle, invProj, sceneView, sceneProj, true);
            }

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
            } else if (gameDestFramebuffer) {
                // No game camera — clear the game output FBO so stale content
                // from previous frames doesn't bleed through behind UI overlays
                s_RenderViewManager->Bind(s_FinalGameOutputHandle);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                s_RenderViewManager->Unbind();
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

    void GraphicsManager::RenderSelectionHighlightForView(
        RenderViewHandle handle,
        const RenderView& view,
        const Math::Mat4& camProj,
        const Math::Mat4& camView,
        const std::vector<DrawCommand>& commands)
    {
        if (handle != s_SceneViewHandle) return;
        if (s_SelectedEntityIds.empty()) return;
        if (s_SelectionOutlineProgram == 0) return;
        if (!view.framebuffer || !view.framebuffer->HasStencil()) return;

        std::vector<const DrawCommand*> selectedCommands;
        selectedCommands.reserve(s_SelectedEntityIds.size());
        for (const auto& command : commands) {
            if (IsSelectedDrawCommand(command)) {
                selectedCommands.push_back(&command);
            }
        }
        if (selectedCommands.empty()) return;

        const GLboolean stencilWasEnabled = glIsEnabled(GL_STENCIL_TEST);
        const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
        const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
        GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        GLboolean depthWriteWasEnabled = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteWasEnabled);

        GLint depthFunc = GL_LESS;
        GLint previousProgram = 0;
        GLint stencilFunc = GL_ALWAYS;
        GLint stencilRef = 0;
        GLint stencilValueMask = 0xFF;
        GLint stencilWriteMask = 0xFF;
        GLint stencilFail = GL_KEEP;
        GLint stencilPassDepthFail = GL_KEEP;
        GLint stencilPassDepthPass = GL_KEEP;
        GLint cullFaceMode = GL_BACK;
        glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
        glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
        glGetIntegerv(GL_STENCIL_FUNC, &stencilFunc);
        glGetIntegerv(GL_STENCIL_REF, &stencilRef);
        glGetIntegerv(GL_STENCIL_VALUE_MASK, &stencilValueMask);
        glGetIntegerv(GL_STENCIL_WRITEMASK, &stencilWriteMask);
        glGetIntegerv(GL_STENCIL_FAIL, &stencilFail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &stencilPassDepthFail);
        glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &stencilPassDepthPass);
        if (cullWasEnabled) {
            glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
        }

        glEnable(GL_STENCIL_TEST);
        glClearStencil(0);
        glClear(GL_STENCIL_BUFFER_BIT);
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        //glEnable(GL_DEPTH_TEST);
        //glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glUseProgram(s_SelectionOutlineProgram->GetProgramID());

        const GLint viewLocation = glGetUniformLocation(s_SelectionOutlineProgram->GetProgramID(), "u_View");
        const GLint projLocation = glGetUniformLocation(s_SelectionOutlineProgram->GetProgramID(), "u_Projection");
        const GLint modelLocation = glGetUniformLocation(s_SelectionOutlineProgram->GetProgramID(), "u_Model");
        const GLint colorLocation = glGetUniformLocation(s_SelectionOutlineProgram->GetProgramID(), "u_Color");
        glUniformMatrix4fv(viewLocation, 1, GL_FALSE, camView.Data());
        glUniformMatrix4fv(projLocation, 1, GL_FALSE, camProj.Data());
        glUniform4f(colorLocation, SELECTION_OUTLINE_COLOR.x, SELECTION_OUTLINE_COLOR.y, SELECTION_OUTLINE_COLOR.z, SELECTION_OUTLINE_COLOR.w);

        for (const DrawCommand* command : selectedCommands) {
            glUniformMatrix4fv(modelLocation, 1, GL_FALSE, command->transform.Data());
            command->mesh->Bind();
            command->mesh->Draw();
            command->mesh->Unbind();
        }

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glStencilMask(0x00);
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        //glDisable(GL_DEPTH_TEST);
        //glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);


        for (const DrawCommand* command : selectedCommands) {
            const Math::Mat4 scaledTransform = command->transform * Math::Mat4::BuildScaling(
                SELECTION_OUTLINE_SCALE,
                SELECTION_OUTLINE_SCALE,
                SELECTION_OUTLINE_SCALE
            );
            glUniformMatrix4fv(modelLocation, 1, GL_FALSE, scaledTransform.Data());
            command->mesh->Bind();
            command->mesh->Draw();
            command->mesh->Unbind();
        }

        if (stencilWasEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
        if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        if (cullWasEnabled) {
            glEnable(GL_CULL_FACE);
            glCullFace(cullFaceMode);
        }
        else {
            glDisable(GL_CULL_FACE);
        }

        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        glDepthMask(depthWriteWasEnabled);
        glDepthFunc(depthFunc);
        glStencilMask(stencilWriteMask);
        glStencilFunc(stencilFunc, stencilRef, stencilValueMask);
        glStencilOp(stencilFail, stencilPassDepthFail, stencilPassDepthPass);
        glUseProgram(previousProgram);
    }

    void GraphicsManager::Submit(const DrawCommand& command) {
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

                NE::Graphics::InstanceData instance{};
                instance.model = command.transform;
                instance.idRGB = command.idRGB;

                instanceData.push_back(instance);
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

        NE::Graphics::GizmosRenderer::Cleanup();
        NE::Graphics::OpenGL::GLGeometryBuffer::ShutdownInstanceBuffer();

        s_LightGizmoQueue.clear();
        s_DecalGizmoQueue.clear();
        s_LightGizmoMesh.reset();
        s_DecalGizmoMaterial.reset();
        s_DecalCubeMesh.reset();
        s_DecalQueue.clear();
        s_NormalPrepassShader.reset();
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

}
