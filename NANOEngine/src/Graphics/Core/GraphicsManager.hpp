#pragma once
#include <memory>
#include "../Interfaces/ICommandBuffer.hpp"
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "../Interfaces/IStateCache.hpp"
#include "Material.hpp"
#include "../../Math/Mat4.hpp"
#include "DrawCommand.hpp"
#include "DrawQueue.hpp"

namespace NE::ECS::Component {
    struct DirectionalLight;
    struct PointLight;
    struct SpotLight;
    struct Light;
}

namespace NE::Graphics {
    class Camera;
    class Skybox;
    class IFrameBuffer;

    struct DebugLine {
        Math::Vec3 from;
        Math::Vec3 to;
        Math::Vec3 color;
    };

    struct DebugTriangle {
        Math::Vec3 v0;
        Math::Vec3 v1;
        Math::Vec3 v2;
        Math::Vec3 color;
    };

    class GraphicsManager {
    public:
        static void Init();
        static void BeginFrame();
        static void DrawSkybox();
		static void DrawFrame();
        static void Submit(const DrawCommand& command);
        static void EndFrame();
        static void Shutdown();

        static void SetCamera(Camera* cam);
        static Camera* GetCamera();

        static uint32_t ReadPixel(IFrameBuffer* framebuffer, uint32_t x, uint32_t y);

        // Gizmo Drawing
        static void InitDebugPrimitives();
        static void AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color);
        static void DrawDebugLines();

        static void AddDebugTriangle(const Math::Vec3& v0, const Math::Vec3& v1, const Math::Vec3& v2, const Math::Vec3& color);
        static void DrawDebugTriangles(); // drawing solid triangles

        // batch functions
        static void AddDebugLinesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color);
        static void AddDebugTrianglesBatch(const std::vector<Math::Vec3>& positions, const Math::Vec3& color);
        static void DrawAllDebugGeometry();

        // UI
        static void DrawUI();

        // lights
        static std::vector<ECS::Component::Light*> m_lights;

        // Draw Count Profiling
        static int drawCount;

        // Flag to toggle sorting
		static bool enableSorting;

    private:
        static std::unique_ptr<ICommandBuffer> s_CommandBuffer;
        static std::unique_ptr<Skybox> s_skybox;
        static Camera* s_ActiveCamera;

        // Gizmo and jolt Drawing
        static std::vector<DebugLine> s_DebugLines;
        static std::vector<DebugTriangle> s_DebugTriangles;

        static std::vector<float> s_DebugVertexBuffer; // pre-allocated buffer to avoid reallocations

        static int s_DebugViewLoc; // cached uniform locations (avoid glGetUniformLocation every frame)
        static int s_DebugProjLoc;

        static constexpr size_t INITIAL_DEBUG_BUFFER_SIZE = 10000;

		// Pipeline state cache
		static std::unique_ptr<IStateCache> s_StateCache;

		// Draw Queue
		static std::unique_ptr<DrawQueue> s_DrawQueue;
    };
}
