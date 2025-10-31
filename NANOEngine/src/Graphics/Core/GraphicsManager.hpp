#pragma once
#include <memory>
#include "../Interfaces/ICommandBuffer.hpp"
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "../Interfaces/IStateCache.hpp"
#include "Material.hpp"
#include "DrawCommand.hpp"
#include "DrawQueue.hpp"

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

    struct CameraData {
        // TODO: camera should know which fbo it is rendering to
        Math::Mat4 projection;
        Math::Mat4 view;
        Math::Vec3 position;
        bool isMain;
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

        // Temp
		static void SetRenderPass(SceneManagement::RenderPass pass);
        static void SubmitPicking(const DrawCommand& command); 
		static void UpdatePicking(); 

        static void SetEditorCamera(EditorCamera* cam);
        static EditorCamera* GetEditorCamera();

		static void SetActiveCamera(const Math::Mat4& projection, const Math::Mat4& view, const Math::Vec3& position, bool isMain);

        static uint32_t ReadPixel(uint32_t x, uint32_t y);

        // Gizmo Drawing
        static void AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color);
        static void DrawDebugLines();

        static std::vector<ECS::Component::Light*> m_lights;

        // Draw Count Profiling
        static int drawCount;

        // Flag to toggle sorting
		static bool enableSorting;

    private:
		static SceneManagement::RenderPass s_CurrentRenderPass; // TEMP?
        static std::unique_ptr<ICommandBuffer> s_CommandBuffer;
        static std::unique_ptr<Skybox> s_skybox;
        static EditorCamera* s_EditorCamera;

		// Note: Each camera should be stored within its own framebuffer in the future
		// Current active camera matrices and position
		static CameraData m_ActiveCamera;

        // Gizmo Drawing
        static std::vector<DebugLine> s_DebugLines;

		// Pipeline state cache
		static std::unique_ptr<IStateCache> s_StateCache;

		// Draw Queue
		static std::unique_ptr<DrawQueue> s_DrawQueue;

    public:
        // Temp, store picking commands
		static std::vector<DrawCommand> s_PickingCommands;

		// Temp, TODO: Create framebuffer registry
		static std::shared_ptr<IFrameBuffer> s_ActiveFrameBuffer;
        static std::shared_ptr<Graphics::IFrameBuffer> s_SceneFrameBuffer;
        static std::shared_ptr<Graphics::IFrameBuffer> s_PickingFrameBuffer;
        static std::shared_ptr<Graphics::IFrameBuffer> s_GameFrameBuffer;
    };

}
