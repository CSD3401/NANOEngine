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
        if (m_numLightsThisView <= 0 || numClusters <= 0)
            return;

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

        uint32_t runningOffset = 0;
        for (int i = 0; i < numClusters; ++i) {
            clusters[i].offset = runningOffset;

            // Clamp so we don't exceed global index pool
            uint32_t count = clusters[i].count;
            if (runningOffset + count > static_cast<uint32_t>(maxClusterLightIndices)) {
                uint32_t remaining = (runningOffset < static_cast<uint32_t>(maxClusterLightIndices))
                    ? (static_cast<uint32_t>(maxClusterLightIndices) - runningOffset)
                    : 0u;
                clusters[i].count = remaining;
                runningOffset += remaining;
                // Optional: warn once
                // SPD_WARNING("ClusteredLighting: global index buffer capacity exceeded; clamping cluster " << i);
            }
            else {
                runningOffset += count;
            }
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
        gpuLights.resize(m_numLightsThisView);

        for (int i = 0; i < m_numLightsThisView; ++i) {
            const auto* src = lights[i];
            auto& dst = gpuLights[i];

            // position.xyz + type
            dst.position[0] = src->position.x;
            dst.position[1] = src->position.y;
            dst.position[2] = src->position.z;
            dst.position[3] = static_cast<float>(src->type);

            // color.rgb + intensity
            dst.color[0] = src->color.x;
            dst.color[1] = src->color.y;
            dst.color[2] = src->color.z;
            dst.color[3] = src->intensity;

            // params:  inner / outer / radius / padding
            dst.params[0] = std::cos(Radians(src->innerCutoff));
            dst.params[1] = std::cos(Radians(src->outerCutoff));
            dst.params[2] = src->radius;
            dst.params[3] = static_cast<float>(src->shadowIndex);

            // direction.xyz + padding
            dst.direction[0] = src->direction.x;
            dst.direction[1] = src->direction.y;
            dst.direction[2] = src->direction.z;
            dst.direction[3] = static_cast<float>(src->shadowType);
        }

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

	// Note: this function is not currently used, as the dispatch is done directly in BuildForView.
	void GLClusteredLighting::DispatchCompute() 
	{
        if (m_numLightsThisView <= 0) return;

        const int numClusters = clustersX * clustersY * clustersZ;
        if (numClusters <= 0) return;

        // Assume compute shader is written with layout(local_size_x = 64)
        constexpr GLuint localSizeX = 64;
        GLuint groupsX = (static_cast<GLuint>(numClusters) + (localSizeX - 1)) / localSizeX;

        glDispatchCompute(groupsX, 1, 1);
	}
}