#pragma once

#include "../Interfaces/IClusteredLighting.hpp"
#include <vector>
#include <memory>

namespace NE::Graphics::OpenGL {

	class GLShader;

    class GLClusteredLighting final : public IClusteredLighting {
    public:
        void Initialize() override;
        void Shutdown() override;
        void OnResize(int width, int height) override;

        void BuildForView(const RenderView& view, const std::vector<Light*>& lights) override;

        void BindForDraw() override;

    private:
        unsigned m_lightSSBO = 0;
        unsigned m_clusterSSBO = 0;
        unsigned m_clusterLightIndicesSSBO = 0;
        unsigned m_clusterParamsUBO = 0;
        unsigned m_lightIndexCounterBuffer = 0;

        int m_clustersX = 16;
        int m_clustersY = 9;
        int m_clustersZ = 24;

        std::shared_ptr<OpenGL::GLShader> m_computeShader;

        // Helper CPU-side structs mirroring GLSL layouts
        struct GPULightCPU { /* position/color/params/direction */ };
        struct ClusterParamsCPU { /* view/proj/zNear/zFar/sizes */ };

        void UploadLights(const std::vector<Light*>& lights, const RenderView& view);
        void DispatchCompute();
    };
}