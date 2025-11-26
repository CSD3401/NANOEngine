#pragma once

#include "../Interfaces/IClusteredLighting.hpp"
#include "Math/Mat4.hpp"
#include "Math/Vec3.hpp"

#include <vector>
#include <memory>

// Forward declarations
namespace NE {
    namespace ECS {
        namespace Component {
            struct Light;
		}
    }
    namespace Graphics {
		struct RenderView;
        namespace OpenGL {
            class GLShader;
        }
	}
}

namespace NE::Graphics::OpenGL {

    class GLClusteredLighting final : public IClusteredLighting {
    public:
		GLClusteredLighting(
            int cX = 16, int cY = 9, int cZ = 24, 
            int maxLights = 1024, int lightIndicesPerCluster = 64
        );
		~GLClusteredLighting();

        void BuildForView(const Graphics::RenderView& view, const std::vector<ECS::Component::Light*>& lights) override;
        void BindForDraw() override;

    private:
		static constexpr int MAX_LIGHTS = 1024;

		// Settings for current view
        int clustersX;
        int clustersY;
        int clustersZ;
        int maxLights;
        int maxClusterLightIndices;

		// Buffer handles
        unsigned m_lightSSBO = 0;
        unsigned m_clusterSSBO = 0;
        unsigned m_clusterLightIndicesSSBO = 0;
        unsigned m_clusterParamsUBO = 0;
        unsigned m_lightIndexCounterBuffer = 0;

        std::shared_ptr<GLShader> m_computeShader;
        int m_numLightsThisView = 0;

        // CPU-side structs matching GLSL layouts
        struct GPULightCPU
        {
            float position[4];  // xyz + type
            float color[4];     // rgb + intensity
            float params[4];    // inner/outer, radius, unused
            float direction[4]; // xyz + (maybe) padding
        };

        struct ClusterParamsCPU
        {
            NE::Math::Mat4 view;  
            NE::Math::Mat4 proj;  

            float zNear;
            float zFar;
            int clustersX;
            int clustersY;

            int clustersZ;
            int numLights;
            int screenWidth;
            int screenHeight;
        };

        void UploadLights(const Graphics::RenderView& view, const std::vector<ECS::Component::Light*>& lights);
        void DispatchCompute();
    };
}