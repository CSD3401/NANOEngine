#pragma once
#include <memory>
#include "../Interfaces/ICommandBuffer.hpp"
#include "../Interfaces/IPipeline.hpp"
#include "../Interfaces/IGeometryBuffer.hpp"
#include "Material.hpp"
#include "../../Math/Mat4.hpp"

namespace NANOEngine::Graphics {
    class Camera;

    struct DrawCommand {
        std::shared_ptr<IGeometryBuffer> mesh;
        std::shared_ptr<Material> material;
        Math::Mat4 transform;
    };

    class GraphicsManager {
    public:
        static void Init();
        static void BeginFrame();
        static void Submit(const DrawCommand& command);
        static void EndFrame();
        static void Shutdown();

        static void SetCamera(Camera* cam);

    private:
        static std::unique_ptr<ICommandBuffer> s_CommandBuffer;
        static Camera* s_ActiveCamera;
    };

}
