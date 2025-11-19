#include "UIRenderer.hpp"
#include "../OpenGL/GLFrameBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include <glad/glad.h>
#include <iostream>
#include <algorithm>

namespace NE::Graphics {

    std::vector<UIDrawCommand> UIRenderer::s_Commands;
    std::unique_ptr<IFrameBuffer> UIRenderer::s_FBO;
    unsigned int UIRenderer::s_VAO = 0;
    unsigned int UIRenderer::s_VBO = 0;
    unsigned int UIRenderer::s_EBO = 0;
    unsigned int UIRenderer::s_Shader = 0;          // Overlay
    unsigned int UIRenderer::s_CameraShader = 0;    // Camera
    unsigned int UIRenderer::s_WorldShader = 0;     // World
    unsigned int UIRenderer::s_CompositeShader = 0;
    unsigned int UIRenderer::s_CompositeVAO = 0;
    unsigned int UIRenderer::s_CompositeVBO = 0;
    uint32_t UIRenderer::s_ScreenW = 0;
    uint32_t UIRenderer::s_ScreenH = 0;


    // shader checks helpers
    static void CheckCompile(GLuint shader, const char* name) {
        GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0'); glGetShaderInfoLog(shader, len, nullptr, log.data());
            std::cout << "[SHADER COMPILE FAIL] " << name << "\n" << log << std::endl;
        }
    }

    static void CheckLink(GLuint prog, const char* name) {
        GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0'); glGetProgramInfoLog(prog, len, nullptr, log.data());
            std::cout << "[PROGRAM LINK FAIL] " << name << "\n" << log << std::endl;
        }
    }

    // overlay mode shader (screen space - pixel coordinates)
    static const char* UIOverlayVertexShader = R"(#version 460 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aUV;

    uniform vec2 uScreenSize;

    out vec2 vUV;

    void main() {
        // Convert pixel coordinates to NDC (-1 to 1)
        float x = (aPos.x / uScreenSize.x) * 2.0 - 1.0;
        float y = 1.0 - (aPos.y / uScreenSize.y) * 2.0;
        gl_Position = vec4(x, y, 0.0, 1.0);
        vUV = aUV;
    })";

    // camera mode shader (screen space with camera)
    static const char* UICameraVertexShader = R"(#version 460 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aUV;

    uniform vec2 uScreenSize;
    uniform mat4 uView;
    uniform mat4 uProj;
    uniform float uPlaneDistance;

    out vec2 vUV;

    void main() {
        // Convert pixel coords to NDC
        float ndcX = (aPos.x / uScreenSize.x) * 2.0 - 1.0;
        float ndcY = 1.0 - (aPos.y / uScreenSize.y) * 2.0;
        
        // Place at distance from camera in view space
        vec4 viewPos = vec4(ndcX * uPlaneDistance, ndcY * uPlaneDistance, -uPlaneDistance, 1.0);
        
        // Transform to clip space
        gl_Position = uProj * viewPos;
        vUV = aUV;
    })";

    // world space shader (full 3d transformation)
    static const char* UIWorldVertexShader = R"(#version 460 core
    layout (location = 0) in vec3 aPos;  // 3D position
    layout (location = 1) in vec2 aUV;

    uniform mat4 uModel;
    uniform mat4 uView;
    uniform mat4 uProj;

    out vec2 vUV;

    void main() {
        gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
        vUV = aUV;
    })";

    // fragment shader (shared by all modes)
    static const char* UIFragmentShader = R"(#version 460 core
    in vec2 vUV;
    out vec4 FragColor;

    uniform vec4 uColor;
    uniform int uUseTexture;
    uniform sampler2D uTex;

    void main() {
        if (uUseTexture == 1) {
            vec4 texColor = texture(uTex, vUV);
            FragColor = texColor * uColor;
        } else {
            FragColor = uColor;
        }
    })";

    void UIRenderer::Init(uint32_t width, uint32_t height) {
        // saves screen size
        s_ScreenW = width;
        s_ScreenH = height;

        // create UI frame buffer
        s_FBO = std::make_unique<OpenGL::GLFrameBuffer>(width, height);

        // check FBO
        {
            s_FBO->Bind(); // binds to its uniquely generated FBO handle

            // which handle?
            GLint currentFBO = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);

            std::cout << "[UI FBO] Created FBO handle: " << s_FBO->GetFramebuffer()
                << ", Currently bound FBO: " << currentFBO << std::endl;

            // check
            GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (st != GL_FRAMEBUFFER_COMPLETE)
            {
                std::cout << "[UI FBO] Incomplete: 0x" << std::hex << st << std::dec << std::endl;
            }

            s_FBO->Unbind(); // back to FBO 0 (default framebuffer)
        }

        // compile overlay shader
        {
            unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &UIOverlayVertexShader, nullptr);
            glCompileShader(vs);
            CheckCompile(vs, "UI Overlay VS");

            unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &UIFragmentShader, nullptr);
            glCompileShader(fs);
            CheckCompile(fs, "UI FS");

            s_Shader = glCreateProgram();
            glAttachShader(s_Shader, vs);
            glAttachShader(s_Shader, fs);
            glLinkProgram(s_Shader);
            CheckLink(s_Shader, "UI Overlay Shader");

            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        // compile camera shader
        {
            unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &UICameraVertexShader, nullptr);
            glCompileShader(vs);
            CheckCompile(vs, "UI Camera VS");

            unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &UIFragmentShader, nullptr);
            glCompileShader(fs);
            CheckCompile(fs, "UI FS");

            s_CameraShader = glCreateProgram();
            glAttachShader(s_CameraShader, vs);
            glAttachShader(s_CameraShader, fs);
            glLinkProgram(s_CameraShader);
            CheckLink(s_CameraShader, "UI Camera Shader");

            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        // compile world space shader
        {
            unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &UIWorldVertexShader, nullptr);
            glCompileShader(vs);
            CheckCompile(vs, "UI World VS");

            unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &UIFragmentShader, nullptr);
            glCompileShader(fs);
            CheckCompile(fs, "UI FS");

            s_WorldShader = glCreateProgram();
            glAttachShader(s_WorldShader, vs);
            glAttachShader(s_WorldShader, fs);
            glLinkProgram(s_WorldShader);
            CheckLink(s_WorldShader, "UI World Shader");

            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        // generate handles for VAO, VBO, EBO
        glGenVertexArrays(1, &s_VAO);
        glGenBuffers(1, &s_VBO);
        glGenBuffers(1, &s_EBO);

        // setup VBO (4 vertices, each with pos(2) + uv(2) = 4 floats)
        glBindVertexArray(s_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, nullptr, GL_DYNAMIC_DRAW);

        // position attribute (location 0) (2D for overlay/camera, 3D for world)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // UV attribute (location 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // setup EBO (indices for 2 triangles = 1 quad)
        unsigned int indices[6] = { 0, 1, 2, 0, 2, 3 };
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // unbind
        glBindVertexArray(0);

        // initialize composite shader
        InitCompositeShader();
    }

    // fullscreen quad vertex shader
    const char* compositeVertexShader = R"(#version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vUV = aUV;
        })";

    // composite fragment shader
    const char* compositeFragmentShader = R"(#version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uUITexture;

        void main() {
            vec4 uiColor = texture(uUITexture, vUV);
            FragColor = uiColor;
        })";

    void UIRenderer::InitCompositeShader() {
        // UI composite shader
        {
            // compile vertex shader
            unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &compositeVertexShader, nullptr);
            glCompileShader(vs);
            CheckCompile(vs, "UI composite VS");

            // compile fragment shader
            unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &compositeFragmentShader, nullptr);
            glCompileShader(fs);
            CheckCompile(fs, "UI commposite VS");

            // link shader program
            s_CompositeShader = glCreateProgram();
            glAttachShader(s_CompositeShader, vs);
            glAttachShader(s_CompositeShader, fs);
            glLinkProgram(s_CompositeShader);
            CheckLink(s_Shader, "UI Program");

            // clean up shaders (no longer needed after linking)
            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        // create fullscreen quad
        float quadVerts[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f, -1.0f,  0.0f, 0.0f
        };

        // upload quad data to GPU
        glGenVertexArrays(1, &s_CompositeVAO);
        glGenBuffers(1, &s_CompositeVBO);

        glBindVertexArray(s_CompositeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_CompositeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // unbind
        glBindVertexArray(0);
    }

    // kiv
    void UIRenderer::SetWindowSize(uint32_t width, uint32_t height) {
        s_ScreenW = width;
        s_ScreenH = height;

        if (s_FBO) 
        {
            s_FBO->Resize(width, height);
        }
    }

    void UIRenderer::BeginFrame() {
        // prepare UI render target before drawing
        if (s_FBO) 
        {
            s_FBO->Bind();

            // set viewport to match UI framebuffer size
            glViewport(0, 0, s_ScreenW, s_ScreenH);

            // clear the framebuffer
            //glClearColor(1, 0, 1, 1); // magenta (temp debug color)
            glClearColor(0, 0, 0, 0); // transparent black
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // check
        static bool printed = false;
        if (!printed)
        {
            std::cout << "[Begin Frame} UI FBO binded" << std::endl;
            printed = true;
        }
    }

    void UIRenderer::Submit(const UIDrawCommand& cmd) {
        s_Commands.push_back(cmd);
    }

    void UIRenderer::EndFrame() {
        if (s_FBO) s_FBO->Unbind();

        // check
        static bool printed = false;
        if (!printed)
        {
            std::cout << "[End Frame} UI FBO unbinded" << std::endl;
            printed = true;
        }
    }

    void UIRenderer::RenderOverlay(const UIDrawCommand& cmd) {
        glUseProgram(s_Shader);
        glUniform2f(glGetUniformLocation(s_Shader, "uScreenSize"), (float)s_ScreenW, (float)s_ScreenH);

        // build quad vertices (2D positions)
        float verts[20] = {
            cmd.x, cmd.y, 0.0f, 0.f, 0.f,                          // top-left
            cmd.x + cmd.width, cmd.y, 0.0f, 1.f, 0.f,              // top-right
            cmd.x + cmd.width, cmd.y + cmd.height, 0.0f, 1.f, 1.f, // bottom-right
            cmd.x, cmd.y + cmd.height, 0.0f, 0.f, 1.f              // bottom-left
        };

        glBindVertexArray(s_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glUniform4f(glGetUniformLocation(s_Shader, "uColor"), cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w);
        glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 0);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void UIRenderer::RenderWithCamera(const UIDrawCommand& cmd) {
        glUseProgram(s_CameraShader);
        glUniform2f(glGetUniformLocation(s_CameraShader, "uScreenSize"), (float)s_ScreenW, (float)s_ScreenH);

        // Pass camera matrices
        glUniformMatrix4fv(glGetUniformLocation(s_CameraShader, "uView"), 1, GL_FALSE, cmd.viewMatrix.Data());
        glUniformMatrix4fv(glGetUniformLocation(s_CameraShader, "uProj"), 1, GL_FALSE, cmd.projMatrix.Data());
        glUniform1f(glGetUniformLocation(s_CameraShader, "uPlaneDistance"), cmd.planeDistance);

        // build quad (pixel coordinates, shader converts to camera space)
        float verts[20] = {
            cmd.x, cmd.y, 0.0f, 0.f, 0.f,
            cmd.x + cmd.width, cmd.y, 0.0f, 1.f, 0.f,
            cmd.x + cmd.width, cmd.y + cmd.height, 0.0f, 1.f, 1.f,
            cmd.x, cmd.y + cmd.height, 0.0f, 0.f, 1.f
        };

        glBindVertexArray(s_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glUniform4f(glGetUniformLocation(s_CameraShader, "uColor"), cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w);
        glUniform1i(glGetUniformLocation(s_CameraShader, "uUseTexture"), 0);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void UIRenderer::RenderWorldSpace(const UIDrawCommand& cmd) {
        glUseProgram(s_WorldShader);

        // build model matrix (position and size in world space)
        Math::Mat4 model = Math::Mat4::BuildTranslation(cmd.x, cmd.y, cmd.z);
        model = model * Math::Mat4::BuildScaling(cmd.width, cmd.height, 1.0f);

        glUniformMatrix4fv(glGetUniformLocation(s_WorldShader, "uModel"), 1, GL_FALSE, model.Data());
        glUniformMatrix4fv(glGetUniformLocation(s_WorldShader, "uView"), 1, GL_FALSE, cmd.viewMatrix.Data());
        glUniformMatrix4fv(glGetUniformLocation(s_WorldShader, "uProj"), 1, GL_FALSE, cmd.projMatrix.Data());

        // build quad (local space: 0-1)
        float verts[20] = {
            0.0f, 0.0f, 0.0f, 0.f, 0.f,
            1.0f, 0.0f, 0.0f, 1.f, 0.f,
            1.0f, 1.0f, 0.0f, 1.f, 1.f,
            0.0f, 1.0f, 0.0f, 0.f, 1.f
        };

        glBindVertexArray(s_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        glUniform4f(glGetUniformLocation(s_WorldShader, "uColor"),
            cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w);
        glUniform1i(glGetUniformLocation(s_WorldShader, "uUseTexture"), 0);

        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    void UIRenderer::DrawFrame() {
        if (s_Commands.empty()) return;

        static bool printed = false;
        if (!printed) {
            std::cout << "[DrawFrame]" << std::endl;
            std::cout << "  Commands queued: " << s_Commands.size() << std::endl;
            std::cout << "  Screen size: " << s_ScreenW << " x " << s_ScreenH << std::endl;

            // verify current FBO
            GLint currentFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO); // get current FBO
            std::cout << "  Drawing to UI FBO: " << currentFBO << std::endl;

            printed = true;
        }

        // sort commands by order
        std::sort(s_Commands.begin(), s_Commands.end(),
            [](const UIDrawCommand& a, const UIDrawCommand& b) {
                return a.order < b.order;
            });

        // save current OpenGL state for 3D scene
        GLboolean depthTest, blend, cullFace;
        GLint blendSrc, blendDst;
        glGetBooleanv(GL_DEPTH_TEST, &depthTest);
        glGetBooleanv(GL_BLEND, &blend);
        glGetBooleanv(GL_CULL_FACE, &cullFace);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

        // set up openGL state for UI rendering
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // render each command based on mode
        for (const auto& cmd : s_Commands) 
        {
            switch (cmd.renderMode) {
            case 0: // overlay
                glDisable(GL_DEPTH_TEST);
                RenderOverlay(cmd);
                break;

            case 1: // camera
                glDisable(GL_DEPTH_TEST);  // or enable for depth with 3D scene
                RenderWithCamera(cmd);
                break;

            case 2: // world space
                glEnable(GL_DEPTH_TEST);   // enable depth for occlusion
                RenderWorldSpace(cmd);
                break;
            }
        }

        // restore state
        glBindVertexArray(0);
        glUseProgram(0);
        if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (!blend) glDisable(GL_BLEND); else glBlendFunc(blendSrc, blendDst);
        if (cullFace) glEnable(GL_CULL_FACE);
    }

    void UIRenderer::ClearCommands() {
        s_Commands.clear();
    }

    IFrameBuffer* UIRenderer::GetFramebuffer() {
        return s_FBO.get();
    }

    void UIRenderer::Composite() {
        if (!s_FBO) return;

        static bool printed = false;
        if (!printed)
        {
            std::cout << "[UIRenderer::Composite] Compositing UI to screen" << std::endl;
            std::cout << "  FBO Color Attachment: " << s_FBO->GetColorAttachment() << std::endl;
            std::cout << "  Screen size: " << s_ScreenW << "x" << s_ScreenH << std::endl;
            printed = true;
        }

        // bind default framebuffer (screen)
        glBindFramebuffer(GL_FRAMEBUFFER, 1);

        // ensure viewport matches screen size
        glViewport(0, 0, s_ScreenW, s_ScreenH);

        // save current OpenGL state for 3D scene
        GLboolean depthTestWasEnabled;
        GLboolean blendWasEnabled;
        GLint blendSrc, blendDst;
        glGetBooleanv(GL_DEPTH_TEST, &depthTestWasEnabled);
        glGetBooleanv(GL_BLEND, &blendWasEnabled);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

        // setup for alpha blending
        glDisable(GL_DEPTH_TEST); // UI has no depth
        glEnable(GL_BLEND); // blend transparency for smoth edges
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // use composite shader
        glUseProgram(s_CompositeShader);

        // bind UI framebuffer texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_FBO->GetColorAttachment()); // FBO’s color attachment is a texture containing your rendered UI
        glUniform1i(glGetUniformLocation(s_CompositeShader, "uUITexture"), 0); // bind to texture unit 0 (kiv to change to bindless)

        // draw fullscreen quad
        glBindVertexArray(s_CompositeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // cleanup
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        // restore old state
        if (depthTestWasEnabled) glEnable(GL_DEPTH_TEST);
        if (!blendWasEnabled)
        {
            glDisable(GL_BLEND);
        }
        else
        {
            glBlendFunc(blendSrc, blendDst);
        }
    }

    //uint32_t UIRenderer::ReadPickId(int x, int y) {
    //    glBindFramebuffer(GL_READ_FRAMEBUFFER, s_PickingFBO->GetFramebuffer());
    //    unsigned char rgba[4] = {};
    //    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    //    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    //    uint32_t id = (uint32_t)rgba[0]
    //        | (uint32_t(rgba[1]) << 8)
    //        | (uint32_t(rgba[2]) << 16)
    //        | (uint32_t(rgba[3]) << 24);
    //    return id;
    //}

    void UIRenderer::Shutdown() {
        // delete OpenGL resources
        if (s_VBO) 
        {
            glDeleteBuffers(1, &s_VBO);
            s_VBO = 0;
        }

        if (s_EBO) 
        {
            glDeleteBuffers(1, &s_EBO);
            s_EBO = 0;
        }

        if (s_VAO) 
        {
            glDeleteVertexArrays(1, &s_VAO);
            s_VAO = 0;
        }

        if (s_Shader) 
        {
            glDeleteProgram(s_Shader);
            s_Shader = 0;
        }

        if (s_CompositeVBO)
        {
            glDeleteBuffers(1, &s_CompositeVBO);
            s_CompositeVBO = 0;
        }

        if (s_CompositeVAO) 
        {
            glDeleteVertexArrays(1, &s_CompositeVAO);
            s_CompositeVAO = 0;
        }

        if (s_CompositeShader) 
        {
            glDeleteProgram(s_CompositeShader);
            s_CompositeShader = 0;
        }

        //if (s_PickingShader)
        //{
        //    glDeleteProgram(s_PickingShader);
        //    s_PickingShader = 0;
        //}
        
        // clear containers
        s_Commands.clear();
        s_FBO.reset();
        //s_PickingFBO.reset();
    }
}
