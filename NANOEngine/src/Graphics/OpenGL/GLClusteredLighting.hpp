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

	// Default clustered lighting settings
	constexpr int MAX_LIGHTS_PER_VIEW = 2048;
	constexpr int CLUSTERED_LIGHTING_SIZE_X = 16;
	constexpr int CLUSTERED_LIGHTING_SIZE_Y = 9;
	constexpr int CLUSTERED_LIGHTING_SIZE_Z = 24;
	constexpr int AVERAGE_CLUSTERS_PER_LIGHT = 16;

    class GLClusteredLighting final : public IClusteredLighting {
    public:
		GLClusteredLighting(
            int cX = CLUSTERED_LIGHTING_SIZE_X, int cY = CLUSTERED_LIGHTING_SIZE_Y, int cZ = CLUSTERED_LIGHTING_SIZE_Z,
			int maxLights = MAX_LIGHTS_PER_VIEW, int avgClustersPerLight = AVERAGE_CLUSTERS_PER_LIGHT
        );
		~GLClusteredLighting();

        void BuildForView(const Graphics::RenderView& view, const std::vector<ECS::Component::Light*>& lights) override;
        void BindForDraw() override;

    private:

        void UploadLights(const Graphics::RenderView& view, const std::vector<ECS::Component::Light*>& lights);

		bool EnsureBufferCapacity(uint32_t required);

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

        std::shared_ptr<GLShader> m_countShader;
        std::shared_ptr<GLShader> m_fillShader;
        int m_numLightsThisView = 0;

		

        // CPU-side structs matching GLSL layouts
		// Any changes here must be reflected in the compute shaders
        struct GPULightCPU
        {
            float position[4];  // xyz + type
            float color[4];     // rgb + intensity
            float params[4];    // inner, outer, radius, unused
            float direction[4]; // xyz + (maybe) padding
        };
        struct ClusterParamsCPU
        {
            NE::Math::Mat4 view;  
            NE::Math::Mat4 proj;  
			NE::Math::Mat4 invProj;

            float zNear;
            float zFar;
            int clustersX;
            int clustersY;
            int clustersZ;

            int numLights;
            int screenWidth;
            int screenHeight;
        };
        struct ClusterGPU {
            uint32_t offset;
            uint32_t count;
            uint32_t pad0;
            uint32_t pad1;
        };

        std::vector<ClusterGPU> m_clustersCPU;
		std::vector<GPULightCPU> m_gpuLightsCPU;
    };
}