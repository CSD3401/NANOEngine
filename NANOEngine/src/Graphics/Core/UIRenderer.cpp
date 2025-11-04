#include "UIRenderer.hpp"
#include "../OpenGL/GLFrameBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include <glad/glad.h>
#include <iostream>

namespace NE::Graphics {

    std::vector<UIDrawCommand> UIRenderer::s_Commands;
    std::unique_ptr<IFrameBuffer> UIRenderer::s_FBO;
    unsigned int UIRenderer::s_VAO = 0;
    unsigned int UIRenderer::s_VBO = 0;
    unsigned int UIRenderer::s_EBO = 0;
    unsigned int UIRenderer::s_Shader = 0;
    unsigned int UIRenderer::s_CompositeShader = 0;
    unsigned int UIRenderer::s_CompositeVAO = 0;
    unsigned int UIRenderer::s_CompositeVBO = 0;
    uint32_t UIRenderer::s_ScreenW = 0;
    uint32_t UIRenderer::s_ScreenH = 0;

    // simple vertex shader (converts pixel coordinates to NDC)
    static const char* UIVertexShaderSource = R"(#version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec2 aUV;

    uniform vec2 uScreenSize;

    out vec2 vUV;

    void main() {
        float x = (aPos.x / uScreenSize.x) * 2.0 - 1.0;
        float y = 1.0 - (aPos.y / uScreenSize.y) * 2.0;
        gl_Position = vec4(x, y, 0.0, 1.0);
        vUV = aUV;
    })";

    // simple fragment shader (draws solid colors or textured quads)
    static const char* UIFragmentShaderSource = R"(#version 330 core
    #extension GL_ARB_bindless_texture : require

    in vec2 vUV;
    out vec4 FragColor;

    layout(bindless_sampler) uniform sampler2D uTex;

    uniform vec4 uColor;
    uniform int uUseTexture; // 0 = solid color, 1 = textured

    void main() {
        if (uUseTexture == 1) 
        {
            vec4 texColor = texture(uTex, vUV);
            FragColor = texColor * uColor; // Tint the texture
        }
        else 
        {
            FragColor = uColor; // Solid color only
        }
    })";

    void UIRenderer::Init(uint32_t width, uint32_t height) {
        s_ScreenW = width;
        s_ScreenH = height;

        // create framebuffer for off screen rendering
        s_FBO = std::make_unique<OpenGL::GLFrameBuffer>(width, height);

        // create VAO for quad rendering
        glGenVertexArrays(1, &s_VAO);
        glGenBuffers(1, &s_VBO);
        glGenBuffers(1, &s_EBO);

        glBindVertexArray(s_VAO);

        // setup VBO (4 vertices, each with pos(2) + uv(2) = 4 floats)
        glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, nullptr, GL_DYNAMIC_DRAW);

        // position attribute (location 0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // UV attribute (location 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // setup EBO (indices for 2 triangles = 1 quad)
        unsigned int indices[6] = { 0, 1, 2, 0, 2, 3 };
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // compile vertex shader
        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &UIVertexShaderSource, nullptr);
        glCompileShader(vs);

        // Compile fragment shader
        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &UIFragmentShaderSource, nullptr);
        glCompileShader(fs);

        // link shader program
        s_Shader = glCreateProgram();
        glAttachShader(s_Shader, vs);
        glAttachShader(s_Shader, fs);
        glLinkProgram(s_Shader);

        // clean up shaders (no longer needed after linking)
        glDeleteShader(vs);
        glDeleteShader(fs);

        // Unbind
        glBindVertexArray(0);

        // initialize composite shader for blitting FBO to screen
        InitCompositeShader();
    }

    void UIRenderer::InitCompositeShader() {
        // fullscreen quad vertex shader
        const char* compositeVertexShaderSource = R"(#version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aUV;
        out vec2 vUV;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vUV = aUV;
        })";

        // composite fragment shader
        const char* compositeFragmentShaderSource = R"(#version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uUITexture;
        void main() {
            vec4 uiColor = texture(uUITexture, vUV);
            FragColor = uiColor;
        })";

        // compile shaders
        unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &compositeVertexShaderSource, nullptr);
        glCompileShader(vs);

        unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &compositeFragmentShaderSource, nullptr);
        glCompileShader(fs);

        s_CompositeShader = glCreateProgram();
        glAttachShader(s_CompositeShader, vs);
        glAttachShader(s_CompositeShader, fs);
        glLinkProgram(s_CompositeShader);

        glDeleteShader(vs);
        glDeleteShader(fs);

        // create fullscreen quad
        glGenVertexArrays(1, &s_CompositeVAO);
        glGenBuffers(1, &s_CompositeVBO);

        glBindVertexArray(s_CompositeVAO);

        float quadVerts[] = {
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
            -1.0f, -1.0f,  0.0f, 0.0f
        };

        glBindBuffer(GL_ARRAY_BUFFER, s_CompositeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void UIRenderer::SetWindowSize(uint32_t width, uint32_t height) {
        s_ScreenW = width;
        s_ScreenH = height;

        // resize framebuffer when window resizes         
        if (s_FBO) 
        {
            s_FBO->Resize(width, height);
        }
    }


    void UIRenderer::BeginFrame() {

        // bind UI framebuffer for off-screen rendering
        if (s_FBO) 
        {
            s_FBO->Bind();

            // Verify FBO is bound correctly
            GLint currentFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
            std::cout << "  Current FBO: " << currentFBO << std::endl;

            glViewport(0, 0, s_ScreenW, s_ScreenH);
        }

        // clear the framebuffer with transparent bg
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        std::cout << "  UI FBO cleared" << std::endl;
    }

    void UIRenderer::Submit(const UIDrawCommand& cmd) {
        s_Commands.push_back(cmd);
    }

    void UIRenderer::DrawFrame() {
       // if (s_Commands.empty()) return;

        static int frameCount = 0;
        static size_t lastCommandCount = 0;
        bool shouldPrint = (frameCount < 5) || (s_Commands.size() != lastCommandCount);

        if (shouldPrint) {
            std::cout << "\n[UIRenderer::DrawFrame] Frame " << frameCount << std::endl;
            std::cout << "  Commands queued: " << s_Commands.size() << std::endl;
            std::cout << "  Screen size: " << s_ScreenW << "x" << s_ScreenH << std::endl;
        }

        if (s_Commands.empty()) {
            if (shouldPrint) {
                std::cout << "  WARNING: No commands to draw!" << std::endl;
            }
            frameCount++;
            lastCommandCount = s_Commands.size();
            return;
        }

        // save current OpenGL state
        GLboolean depthTestWasEnabled;
        GLboolean blendWasEnabled;
        GLint blendSrc, blendDst;

        glGetBooleanv(GL_DEPTH_TEST, &depthTestWasEnabled);
        glGetBooleanv(GL_BLEND, &blendWasEnabled);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

        // set up openGL state for UI rendering
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // use UI shader
        glUseProgram(s_Shader);

        // set screen size uniform
        glUniform2f(glGetUniformLocation(s_Shader, "uScreenSize"), (float)s_ScreenW, (float)s_ScreenH);

        // bind VAO
        glBindVertexArray(s_VAO);

        // draw each command
        int cmdIndex = 0;
        for (const auto& cmd : s_Commands)
        {
            if (shouldPrint) {
                std::cout << "  Drawing command " << cmdIndex << ": "
                    << "pos(" << cmd.x << "," << cmd.y << ") "
                    << "size(" << cmd.width << "," << cmd.height << ") "
                    << "color(" << cmd.color.x << "," << cmd.color.y << ","
                    << cmd.color.z << "," << cmd.color.w << ")" << std::endl;
            }

            // build quad vertices in pixel space
            float x = cmd.x;
            float y = cmd.y;
            float w = cmd.width;
            float h = cmd.height;

            // vertex data: [x, y, u, v] for each corner
            float verts[16] = {
                x,     y,     0.f, 0.f, // top-left
                x + w, y,     1.f, 0.f, // top-right
                x + w, y + h, 1.f, 1.f, // bottom-right
                x,     y + h, 0.f, 1.f  // bottom-left
            };

            // upload vertices to GPU
            glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

            // set color uniform
            glUniform4f(glGetUniformLocation(s_Shader, "uColor"), cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w);

            // handle texture if material exists
            if (cmd.material && !cmd.material->GetTextures().empty()) 
            {
                // get the first texture from the material's texture map
                const auto& textures = cmd.material->GetTextures();
                auto it = textures.begin();
                if (it != textures.end() && it->second) 
                {
                    uint64_t handle = it->second->GetBindlessHandle();
                    it->second->MakeResident();
                    glUniformHandleui64ARB(glGetUniformLocation(s_Shader, "uTex"), handle);
                    glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 1);
                }
                else 
                {
                    // material has no valid textures, use solid color
                    glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 0);
                }
            }
            else
            {
                // no material or no textures, just solid color
                glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 0);
            }

            // draw the quad
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            cmdIndex++;
        }

        if (shouldPrint) {
            std::cout << "  Drew " << cmdIndex << " UI elements" << std::endl;
        }

        // restore OpenGL state
        glBindVertexArray(0);
        glUseProgram(0);

        if (depthTestWasEnabled) 
        {
            glEnable(GL_DEPTH_TEST);
        }

        if (!blendWasEnabled)
        {
            glDisable(GL_BLEND);
        }
        else
        {
            glBlendFunc(blendSrc, blendDst);
        }

        frameCount++;
        lastCommandCount = s_Commands.size();
    }

    void UIRenderer::EndFrame() {
        if (s_FBO) s_FBO->Unbind();
    }

    void UIRenderer::ClearCommands() {
        s_Commands.clear();
    }

    IFrameBuffer* UIRenderer::GetFramebuffer() {
        return s_FBO.get();
    }

    void UIRenderer::Composite() {
        if (!s_FBO) return;

        std::cout << "[UIRenderer::Composite] Compositing UI to screen" << std::endl;
        std::cout << "  FBO Color Attachment: " << s_FBO->GetColorAttachment() << std::endl;
        std::cout << "  Screen size: " << s_ScreenW << "x" << s_ScreenH << std::endl;

        // bind default framebuffer (screen)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Get current viewport to verify
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        std::cout << "  Current viewport: " << viewport[0] << "," << viewport[1]
            << " " << viewport[2] << "x" << viewport[3] << std::endl;

        // restore viewport to screen size
        glViewport(0, 0, s_ScreenW, s_ScreenH);

        // setup for alpha blending
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // use composite shader
        glUseProgram(s_CompositeShader);

        // bind UI framebuffer texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_FBO->GetColorAttachment());
        glUniform1i(glGetUniformLocation(s_CompositeShader, "uUITexture"), 0);

        // draw fullscreen quad
        glBindVertexArray(s_CompositeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Check for OpenGL errors
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cout << "  ERROR: OpenGL error during composite: " << err << std::endl;
        }
        else {
            std::cout << "  Composite complete (no errors)" << std::endl;
        }

        // cleanup
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);

        // restore depth test
        glEnable(GL_DEPTH_TEST);
    }

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
        
        // clear containers
        s_Commands.clear();
        s_FBO.reset();
    }
}
