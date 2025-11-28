#pragma once
#include <memory>
#include "../Interfaces/ICommandBuffer.hpp"
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "../Interfaces/IStateCache.hpp"
#include "Material.hpp"
#include "DrawCommand.hpp"
#include "DrawQueue.hpp"
#include "RenderViewManager.hpp"
#include "RenderSettings.hpp"
#include "PostProcessingSettings.hpp"

// Forward declarations
namespace NE::ECS::Component {
    struct DirectionalLight;
    struct PointLight;
    struct SpotLight;
    struct Light;
}
namespace NE::SceneManagement {
    enum class RenderPass;
}

namespace NE::Graphics {
    class EditorCamera;
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
        static void SubmitSkybox();
		static void DrawFrame();
        static void Submit(const DrawCommand& command);
        static void EndFrame();
        static void Clear();
        static void Shutdown();

        static void SetEditorCamera(EditorCamera* cam);
        static EditorCamera* GetEditorCamera();
        static void UpdateEditorCameraData();

        static uint32_t GetScreenWidth();
        static uint32_t GetScreenHeight();
        static IStateCache* GetStateCache();

		static void SetActiveCamera(const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, bool isMain);
        
		static RenderViewHandle CreateRenderView(uint32_t width, uint32_t height, bool enablePicking = true);
        static void SetCameraData(RenderViewHandle viewHandle, const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, bool isMain, uint16_t order);
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

        // UI
        static void DrawUI();

        // lights
        static std::vector<ECS::Component::Light*> m_lights;

        // Draw Count Profiling
        static int drawCount;

        // Flag to toggle sorting
		static bool enableSorting;

		// Render View Handles
        static RenderViewHandle s_SceneViewHandle;
        static RenderViewHandle s_FinalOutputViewHandle;
        static RenderViewHandle s_GameViewHandle;

        static RenderSettings renderSettings;
        // Experimental here for now
        static PostProcessingSettings postProcessingSettings;
    private:
        static uint32_t s_ScreenWidth;
        static uint32_t s_ScreenHeight;

		static SceneManagement::RenderPass s_CurrentRenderPass; // TEMP?

        
        static std::unique_ptr<ICommandBuffer> s_CommandBuffer;
        static std::unique_ptr<Skybox> s_skybox;
        static EditorCamera* s_EditorCamera;

		// Note: Each camera should be stored within its own framebuffer in the future
		// Current active camera matrices and position
		//static CameraData m_ActiveCamera;

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

		// Framebuffer Manager
		static std::unique_ptr<RenderViewManager> s_RenderViewManager;
		static RenderViewHandle s_ActiveViewHandle;
    };
}
