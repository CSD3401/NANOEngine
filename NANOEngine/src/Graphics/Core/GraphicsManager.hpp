#pragma once

#include "../../Math/Vec3.hpp"
#include <memory>
#include "../Interfaces/ICommandBuffer.hpp"
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "../Interfaces/IStateCache.hpp"
#include "Material.hpp"
#include "DrawCommand.hpp"
#include "DecalCommand.hpp"
#include "DecalGizmoCommand.hpp"
#include "DrawQueue.hpp"
#include "RenderViewManager.hpp"
#include "RenderSettings.hpp"
#include "PostProcessingSettings.hpp"
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
        class RenderGraph;
        class TexturePool;
        class PostProcessPipeline;
        struct DrawCommand;
        struct LightGizmoCommand;
        struct RenderView;
        struct RenderSettings;

        using RenderViewHandle = std::uint32_t;

        class ShadowRenderer;
        namespace OpenGL {
            class GLShader;
        }
	}
    namespace ECS {
        namespace Component {
            struct Light;
        }
	}
    namespace Math {
        struct Vec3;
		struct Mat4;
	}
}

namespace NE::Graphics {
    class GraphicsManager {
    public:
        static void Init();

        static void BeginFrame();
		static void DrawFrame();
        static void Submit(const DrawCommand& command);
        static void SubmitDecal(const DecalCommand& command);
        static void SubmitDecalGizmo(const DecalGizmoCommand& command);
        static void SubmitLightGizmo(const LightGizmoCommand& command);
        static void EndFrame();
        static void Clear();
        static void Shutdown();

        static void SetEditorCamera(EditorCamera* cam);
        static EditorCamera* GetEditorCamera();
        static void UpdateEditorCameraData();

        static uint32_t GetScreenWidth();
        static uint32_t GetScreenHeight();
        static IStateCache* GetStateCache();
        
		static RenderViewHandle CreateRenderView(uint32_t width, uint32_t height, bool enablePicking = true);
        static void DestroyRenderView(RenderViewHandle handle);
        static void SetCameraData(RenderViewHandle viewHandle, const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, float nearPlane, float farPlane, bool isMain, uint16_t order);
		static void EnableCamera(RenderViewHandle viewHandle);
		static void DisableCamera(RenderViewHandle viewHandle);

        static uint32_t ReadPixel(uint32_t x, uint32_t y);
        static void ReadPixelRect(uint32_t x, uint32_t y,
            uint32_t width, uint32_t height,
            std::vector<uint32_t>& outIds);

		// Used for ImGui texture display
		static uint32_t GetSceneColorAttachment();
		static uint32_t GetGameColorAttachment();

		// Used to get final output for fullscreen display
		static uint32_t GetFinalOutputColorAttachment();

		// Display final output to screen
		static void DisplayFinalOutput(int windowWidth, int windowHeight);

        // Gizmo Drawing
        static void InitDebugPrimitives();
        static void AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color);
        static void DrawDebugLines();

        static void AddDebugTriangle(const Math::Vec3& v0, const Math::Vec3& v1, const Math::Vec3& v2, const Math::Vec3& color);
        static void DrawDebugTriangles(); // drawing solid triangles

		static void DrawSelectedLightGizmos(const ECS::Component::Light& light);

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
        static RenderViewHandle s_FinalGameOutputHandle;
        static RenderViewHandle s_GameViewHandle;

        static RenderSettings renderSettings;
        // Experimental here for now
        static PostProcessingSettings postProcessingSettings;

        // Render Graph
        static RenderGraph* GetRenderGraph();
        static TexturePool* GetTexturePool();

    private:
        static uint32_t s_ScreenWidth;
        static uint32_t s_ScreenHeight;

        // Command Buffer
        static std::unique_ptr<ICommandBuffer> s_CommandBuffer;

		// Skybox
        static std::unique_ptr<Skybox> s_skybox;

		// Editor Camera
        static EditorCamera* s_EditorCamera;

		// Pipeline state cache
		static std::unique_ptr<IStateCache> s_StateCache;

		// Draw Queue
		static std::unique_ptr<DrawQueue> s_DrawQueue;
        static std::vector<DecalCommand> s_DecalQueue;

		// Framebuffer Manager
		static std::unique_ptr<RenderViewManager> s_RenderViewManager;
		//static RenderViewHandle s_ActiveViewHandle;

		// Clustered Lighting System for forward+ rendering
        static std::shared_ptr<IClusteredLighting> s_clusteredLighting;

        // Post-processing
        static std::unique_ptr<PostProcessPipeline> s_PostPipeline;
        static std::shared_ptr<OpenGL::GLShader> s_NormalPrepassShader;
        
        static std::unique_ptr<ShadowRenderer> s_shadowRenderer;

    };
}
