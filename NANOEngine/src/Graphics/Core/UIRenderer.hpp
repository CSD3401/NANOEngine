#ifndef UI_RENDERER_HPP
#define UI_RENDERER_HPP

#include <vector>
#include <memory>
#include "UIDrawCommand.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include <glad/glad.h>

namespace NE::Graphics {

    class UIRenderer {
    public:
        static void Init(uint32_t width, uint32_t height);
        static void Submit(const UIDrawCommand& cmd);
        static void ClearCommands();

        static void BeginFrame();
        static void EndFrame();
        static void DrawUIFrame();
        static void Draw3DUIFrame(GLuint targetFBO);
        static void Composite(uint32_t targetFBO = 0);
        static void Shutdown();

        static void DrawTestQuad();

        static IFrameBuffer* GetFramebuffer(); // for compositing

    private:
        // openGl resources
        static std::vector<UIDrawCommand> s_Commands;
        static std::unique_ptr<IFrameBuffer> s_FBO; // GLFrameBuffer
        static unsigned int s_VAO, s_VBO, s_EBO;
        static unsigned int s_Shader;
        static unsigned int s_CompositeShader;
        static unsigned int s_CompositeVAO, s_CompositeVBO;
        static uint32_t s_ScreenW, s_ScreenH;

        static void InitCompositeShader();

        // material helpers
        static void BuildQuadVertices(const UIDrawCommand& cmd, float* verts);
    };

} // namespace NE::Graphics
#endif // END UI_RENDERER_HPP
