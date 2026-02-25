#include "pch.h"
#include "GLClusteredLighting.hpp"

#include "../Core/RenderViewManager.hpp"
#include "../../ECS/Components/Light.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include "../Interfaces/IShader.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLFrameBuffer.hpp"
#include <glad/glad.h>

namespace {
    float Radians(float deg) {
        return deg * 3.14159265358979323846f / 180.0f;
    }
}

namespace NE::Graphics::OpenGL {

    static uint32_t NextPow2(uint32_t v) {
        if (v == 0) return 1;
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;
        return v;
    }

    GLClusteredLighting::GLClusteredLighting(int cX, int cY, int cZ, int maxLights, int avgClustersPerLight)
		: clustersX(cX), clustersY(cY), clustersZ(cZ),
          maxLights(maxLights), maxClusterLightIndices(maxLights * avgClustersPerLight)
    {
        m_countShader = Resource::ResourceManager::GetInstance().LoadResource<GLShader>("nefpcount");
        m_fillShader = Resource::ResourceManager::GetInstance().LoadResource<GLShader>("nefpfill");

        // Lights SSBO (binding = 0 in GLSL)
        if (!m_lightSSBO) {
            glGenBuffers(1, &m_lightSSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                sizeof(GPULightCPU) * maxLights,
                nullptr,
                GL_DYNAMIC_DRAW);
        }

        // Clusters SSBO (binding = 1 in GLSL)
        if (!m_clusterSSBO) {
            glGenBuffers(1, &m_clusterSSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_clusterSSBO);
            // Simple placeholder size: 4 uints per cluster (offset, count, padding...)
            const int numClusters = clustersX * clustersY * clustersZ;
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                sizeof(uint32_t) * 4 * numClusters,
                nullptr,
                GL_DYNAMIC_DRAW);
        }

        // Cluster light indices SSBO (binding = 2 in GLSL)
        if (!m_clusterLightIndicesSSBO) {
            glGenBuffers(1, &m_clusterLightIndicesSSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_clusterLightIndicesSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                sizeof(uint32_t) * maxClusterLightIndices,
                nullptr,
                GL_DYNAMIC_DRAW);
        }

        // Cluster params UBO (binding = 3 in GLSL)
        if (!m_clusterParamsUBO) {
            glGenBuffers(1, &m_clusterParamsUBO);
            glBindBuffer(GL_UNIFORM_BUFFER, m_clusterParamsUBO);
            glBufferData(GL_UNIFORM_BUFFER,
                sizeof(ClusterParamsCPU),
                nullptr,
                GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_clusterParamsUBO);
        }

        // Atomic counter (binding = 0 in GLSL atomic counter space)
        if (!m_lightIndexCounterBuffer) {
            glGenBuffers(1, &m_lightIndexCounterBuffer);
            glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, m_lightIndexCounterBuffer);
            GLuint zero = 0;
            glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);
            glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, m_lightIndexCounterBuffer);
        }

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    }

    GLClusteredLighting::~GLClusteredLighting() 
    {
        m_countShader.reset();
        m_fillShader.reset();
        if (m_lightSSBO) {
            glDeleteBuffers(1, &m_lightSSBO);
            m_lightSSBO = 0;
        }
        if (m_clusterSSBO) {
            glDeleteBuffers(1, &m_clusterSSBO);
            m_clusterSSBO = 0;
        }
        if (m_clusterLightIndicesSSBO) {
            glDeleteBuffers(1, &m_clusterLightIndicesSSBO);
            m_clusterLightIndicesSSBO = 0;
        }
        if (m_clusterParamsUBO) {
            glDeleteBuffers(1, &m_clusterParamsUBO);
            m_clusterParamsUBO = 0;
        }
        if (m_lightIndexCounterBuffer) {
            glDeleteBuffers(1, &m_lightIndexCounterBuffer);
            m_lightIndexCounterBuffer = 0;
        }
    }

	void GLClusteredLighting::BuildForView(const Graphics::RenderView& view, const std::vector<ECS::Component::Light*>& lights)
	{
        if (!m_countShader || !m_fillShader) {
            return;
        }

        m_numLightsThisView = static_cast<int>(lights.size());
        if (m_numLightsThisView > maxLights) {
            SPD_WARNING("GLClusteredLighting::BuildForView: clamping lights from "
                << m_numLightsThisView << " to " << maxLights);
            m_numLightsThisView = maxLights;
        }

        UploadLights(view, lights);

        const int numClusters = clustersX * clustersY * clustersZ;
		// Early out if no lights
		// Note: we still call UploadLights so the fragment shaders know the light count
        if (m_numLightsThisView <= 0 || numClusters <= 0) {
            return;
        }
            

        // ---------- PASS 1: COUNT ----------
        // Bind SSBOs
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_lightSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_clusterSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_clusterLightIndicesSSBO);

        // Bind UBO
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_clusterParamsUBO);

        m_countShader->Bind();

        constexpr GLuint localSizeX = 64;
        GLuint groupsX = (static_cast<GLuint>(numClusters) + (localSizeX - 1)) / localSizeX;
        glDispatchCompute(groupsX, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // ---------- CPU PREFIX SUM ----------
        std::vector<ClusterGPU> clusters(numClusters);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_clusterSSBO);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,
            0,
            sizeof(ClusterGPU) * numClusters,
            clusters.data());

        // Compute required total indices (sum of counts)
        uint32_t required = 0;
        uint32_t largestCount = 0;
        for (int i = 0; i < numClusters; ++i) {
            largestCount = std::max(largestCount, clusters[i].count);
            required += clusters[i].count;
        }

		// If we exceeded the allocated size for cluster light indices, reallocate and re-run
        if (EnsureBufferCapacity(required)) {

            // Re-run PASS 1: COUNT (because we just reallocated SSBO binding=2)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_lightSSBO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_clusterSSBO);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_clusterLightIndicesSSBO);
            glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_clusterParamsUBO);

            m_countShader->Bind();
            glDispatchCompute(groupsX, 1, 1);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

            // Read counts again
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_clusterSSBO);
            glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,
                0,
                sizeof(ClusterGPU) * numClusters,
                clusters.data());
        }

        // Prefix sum offsets
        uint32_t runningOffset = 0;
        for (int i = 0; i < numClusters; ++i) {
            clusters[i].offset = runningOffset;
            runningOffset += clusters[i].count;
        }

        // Upload offsets (and possibly clamped counts) back to GPU
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
            0,
            sizeof(ClusterGPU) * numClusters,
            clusters.data());

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // ---------- PASS 2: FILL ----------
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_lightSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_clusterSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_clusterLightIndicesSSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_clusterParamsUBO);

        m_fillShader->Bind();

        glDispatchCompute(groupsX, 1, 1);

        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	void GLClusteredLighting::BindForDraw() 
	{
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_lightSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_clusterSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_clusterLightIndicesSSBO);
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_clusterParamsUBO);
	}

    void GLClusteredLighting::UploadLights(const Graphics::RenderView& view, const std::vector<ECS::Component::Light*>& lights)
    {
        using NE::Math::Mat4;
        using NE::Math::Vec3;

        // ---- Lights SSBO ----
        std::vector<GPULightCPU> gpuLights;
        gpuLights.reserve(m_numLightsThisView);

        auto pushLight = [&](const ECS::Component::Light* src,
            float intensity,
            float radius,
            float innerCos,
            float outerCos)
            {
                GPULightCPU dst{};

                dst.position[0] = src->position.x;
                dst.position[1] = src->position.y;
                dst.position[2] = src->position.z;
                dst.position[3] = (float)src->type;

                dst.color[0] = src->color.x;
                dst.color[1] = src->color.y;
                dst.color[2] = src->color.z;
                dst.color[3] = intensity;

                dst.params[0] = innerCos;
                dst.params[1] = outerCos;
                dst.params[2] = radius; // range/radius
                dst.params[3] = (float)src->shadowIndex;

                auto dir = src->direction.Normalized();
                dst.direction[0] = dir.x;
                dst.direction[1] = dir.y;
                dst.direction[2] = dir.z;
                dst.direction[3] = (float)src->shadowType;

                gpuLights.push_back(dst);
        };

		for (int i = 0; i < m_numLightsThisView; ++i) {
			const auto* src = lights[i];
            // Extract per-type values from variant
            bool keep = true;

            float intensity = 0.0f;
            float radius = 0.0f;
            float innerCos = 0.0f;
            float outerCos = 0.0f;

            std::visit([&](const auto& d) {
                using T = std::decay_t<decltype(d)>;

                // intensity exists on all your payload structs
                intensity = d.intensity;

                if constexpr (std::is_same_v<T, ECS::Component::Light::SpotLightData>) {
                    // Degrees -> radians -> cos
                    innerCos = std::cos(Radians(d.innerConeAngleDeg));
                    outerCos = std::cos(Radians(d.outerConeAngleDeg));
                    radius = d.range;
                } else if constexpr (std::is_same_v<T, ECS::Component::Light::PointLightData>) {
                    radius = d.range;
                } else if constexpr (std::is_same_v<T, ECS::Component::Light::AreaLightData>) {
                    radius = d.range;
                    // width/height not used in your current packed struct; see note below
                } else if constexpr (std::is_same_v<T, ECS::Component::Light::DirectionalLightData>) {
                    // no range; keep params[2] = 0
                    radius = 0.0f;
                }
            }, src->data);

            // --- discard rules ---
            if (intensity <= 0.0f) keep = false;

            // only apply radius rule to non-directional
            if (keep && src->type != ECS::Component::Light::Type::Directional) {
                if (radius <= 0.0f) keep = false;
            }

            if (!keep) continue;

            pushLight(src, intensity, radius, innerCos, outerCos);

            if (static_cast<int>(gpuLights.size()) >= maxLights)
                break;
        }

        m_numLightsThisView = static_cast<int>(gpuLights.size());

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_lightSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
            0,
            gpuLights.size() * sizeof(GPULightCPU),
            gpuLights.data());

        // ---- Cluster params UBO ----
        ClusterParamsCPU params{};
        params.view = view.view;
        params.proj = view.projection;
        params.invProj = view.projection.Inverse();
        params.zNear = view.nearPlane;
        params.zFar = view.farPlane;

        params.clustersX = clustersX;
        params.clustersY = clustersY;
        params.clustersZ = clustersZ;
        params.numLights = m_numLightsThisView;
        params.screenWidth = static_cast<int>(view.framebuffer->GetWidth());
        params.screenHeight = static_cast<int>(view.framebuffer->GetHeight());

        glBindBuffer(GL_UNIFORM_BUFFER, m_clusterParamsUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(ClusterParamsCPU), &params);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    bool GLClusteredLighting::EnsureBufferCapacity(uint32_t required) 
    {
        if (required <= (uint32_t)maxClusterLightIndices)
            return false;

        // grow with headroom to reduce frequent reallocs
        uint32_t newCap = NextPow2((uint32_t)std::ceil(required * 1.25f));

        SPD_WARNING("ClusteredLighting: growing index pool from "
            << maxClusterLightIndices << " -> " << newCap
            << " (required=" << required << ")");

        maxClusterLightIndices = (int)newCap;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_clusterLightIndicesSSBO);
        glBufferData(GL_SHADER_STORAGE_BUFFER,
            sizeof(uint32_t) * (size_t)maxClusterLightIndices,
            nullptr,
            GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        return true;
	}
}