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
        static void Shutdown();

        static void InitCompositeShader();
        static void Composite();

        static void BeginFrame();
        static void Submit(const UIDrawCommand& cmd);
        static void DrawFrame();
        static void EndFrame();

        static void SetWindowSize(uint32_t width, uint32_t height);

        static IFrameBuffer* GetFramebuffer(); // for compositing

    private:
        static std::vector<UIDrawCommand> s_Commands;
        static std::unique_ptr<IFrameBuffer> s_FBO; // GLFrameBuffer
        static unsigned int s_VAO, s_VBO, s_EBO;
        static unsigned int s_Shader;
        static unsigned int s_CompositeVAO, s_CompositeVBO;
        static unsigned int s_CompositeShader;
        static uint32_t s_ScreenW, s_ScreenH;
    };

} // namespace NE::Graphics
#endif // END UI_RENDERER_HPP
