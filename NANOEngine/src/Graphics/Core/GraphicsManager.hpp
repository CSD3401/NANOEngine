#pragma once
#include <memory>
#include "../Interfaces/ICommandBuffer.hpp"
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "Material.hpp"
#include "../../Math/Mat4.hpp"

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

    struct DrawCommand {
        std::shared_ptr<IGeometryBuffer> mesh;
        std::shared_ptr<Material> material;
        Math::Mat4 transform;
    };

    class GraphicsManager {
    public:
        static void Init();
        static void BeginFrame();
        static void DrawSkybox();
        static void Submit(const DrawCommand& command);
        static void EndFrame();
        static void Shutdown();

        static void SetCamera(Camera* cam);

        static uint32_t ReadPixel(IFrameBuffer* framebuffer, uint32_t x, uint32_t y);

        // Gizmo Drawing
        static void AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color);
        static void DrawDebugLines();

        static std::vector<ECS::Component::Light*> m_lights;
    private:
        static std::unique_ptr<ICommandBuffer> s_CommandBuffer;
        static std::unique_ptr<Skybox> s_skybox;
        static Camera* s_ActiveCamera;

        // Gizmo Drawing
        static std::vector<DebugLine> s_DebugLines;
    };

}
