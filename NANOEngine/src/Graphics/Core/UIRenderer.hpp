#ifndef UI_RENDERER_HPP
#define UI_RENDERER_HPP

#include <vector>
#include <memory>
#include "UIDrawCommand.hpp"
#include "../Interfaces/IFrameBuffer.hpp"

namespace NE::Graphics {

    class UIRenderer {
    public:
        static void Init(uint32_t width, uint32_t height);
        static void SetWindowSize(uint32_t width, uint32_t height);

        static void BeginFrame();
        static void Submit(const UIDrawCommand& cmd);
        static void DrawFrame();
        static void EndFrame();
        static void ClearCommands();
        static void Composite();
        static void Shutdown();


        static IFrameBuffer* GetFramebuffer(); // for compositing

    private:
        static void InitCompositeShader();
        static void InitWorldSpaceShader();

        // render methods for different modes
        static void RenderOverlay(const UIDrawCommand& cmd);
        static void RenderWithCamera(const UIDrawCommand& cmd);
        static void RenderWorldSpace(const UIDrawCommand& cmd);

        // openGl resources
        static std::vector<UIDrawCommand> s_Commands;
        static std::unique_ptr<IFrameBuffer> s_FBO; // GLFrameBuffer
        static unsigned int s_VAO, s_VBO, s_EBO;
        static unsigned int s_Shader;               // Overlay shader
        static unsigned int s_CameraShader;         // Camera mode shader
        static unsigned int s_WorldShader;          // World space shader
        static unsigned int s_CompositeShader;
        static unsigned int s_CompositeVAO, s_CompositeVBO;
        static uint32_t s_ScreenW, s_ScreenH;
    };

} // namespace NE::Graphics
#endif // END UI_RENDERER_HPP
