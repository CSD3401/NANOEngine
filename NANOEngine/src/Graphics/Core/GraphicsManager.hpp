#pragma once

#include "../../Math/Vec3.hpp"
#include <memory>
#include <vector>


// Forward declarations
namespace NE {
    namespace Graphics {
        class EditorCamera;
        class Skybox;
        class IFrameBuffer;
        class IClusteredLighting;
        class IStateCache;
        class ICommandBuffer;
        class DrawQueue;
        class RenderViewManager;
        struct DrawCommand;
        struct RenderView;
        struct RenderSettings;

        using RenderViewHandle = std::uint32_t;
	}
    namespace ECS {
        namespace Component {
            struct DirectionalLight;
            struct PointLight;
            struct SpotLight;
            struct Light;
        }
	}
    namespace Math {
        struct Vec3;
		struct Mat4;
	}
}

namespace NE::Graphics {

    // Debug needs to be moved
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
        static void SubmitSkybox();
		static void DrawFrame();
        static void Submit(const DrawCommand& command);
        static void EndFrame();
        static void Clear();
        static void Shutdown();

        static void SetEditorCamera(EditorCamera* cam);
        static EditorCamera* GetEditorCamera();
        static void UpdateEditorCameraData();

		static RenderViewHandle CreateRenderView(uint32_t width, uint32_t height, bool enablePicking = true);
        static void SetCameraData(RenderViewHandle viewHandle, const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, float nearPlane, float farPlane, bool isMain, uint16_t order);
		static void EnableCamera(RenderViewHandle viewHandle);
		static void DisableCamera(RenderViewHandle viewHandle);

        static uint32_t ReadPixel(uint32_t x, uint32_t y);

		// Used for ImGui texture display
		static uint32_t GetSceneColorAttachment();
		static uint32_t GetGameColorAttachment();

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

        static std::vector<ECS::Component::Light*> m_lights;

        // Draw Count Profiling
        static int drawCount;

        // Flag to toggle sorting
		static bool enableSorting;

		// Render View Handles
        static RenderViewHandle s_SceneViewHandle;
        static RenderViewHandle s_GameViewHandle;

        static RenderSettings renderSettings;

    private:
		// Command Buffer
        static std::unique_ptr<ICommandBuffer> s_CommandBuffer;

		// Skybox
        static std::unique_ptr<Skybox> s_skybox;

		// Editor Camera
        static EditorCamera* s_EditorCamera;

        // Gizmo and jolt Drawing
        static std::vector<DebugLine> s_DebugLines;
        static std::vector<DebugTriangle> s_DebugTriangles;

		// Pipeline state cache
		static std::unique_ptr<IStateCache> s_StateCache;

		// Draw Queue
		static std::unique_ptr<DrawQueue> s_DrawQueue;

		// Framebuffer Manager
		static std::unique_ptr<RenderViewManager> s_RenderViewManager;
		static RenderViewHandle s_ActiveViewHandle;

		// Clustered Lighting System for forward+ rendering
        static std::shared_ptr<IClusteredLighting> s_clusteredLighting;

        // Debug
        static std::vector<float> s_DebugVertexBuffer; // pre-allocated buffer to avoid reallocations
        static int s_DebugViewLoc; // cached uniform locations (avoid glGetUniformLocation every frame)
        static int s_DebugProjLoc;
        static constexpr size_t INITIAL_DEBUG_BUFFER_SIZE = 10000;
    };

}
