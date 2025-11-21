#include "UIRenderer.hpp"
#include "../OpenGL/GLFrameBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../src/Graphics/Core/PipelineCache.hpp"
#include "../src/ResourceManagement/ResourceManager.hpp"
#include "../src/Graphics/Core/GraphicsManager.hpp"
#include <glad/glad.h>
#include <iostream>
#include <algorithm>

namespace NE::Graphics {

    std::vector<UIDrawCommand> UIRenderer::s_Commands;
    std::unique_ptr<IFrameBuffer> UIRenderer::s_FBO;
    unsigned int UIRenderer::s_VAO = 0;
    unsigned int UIRenderer::s_VBO = 0;
    unsigned int UIRenderer::s_EBO = 0;
    unsigned int UIRenderer::s_CompositeShader = 0;
    unsigned int UIRenderer::s_CompositeVAO = 0;
    unsigned int UIRenderer::s_CompositeVBO = 0;
    uint32_t UIRenderer::s_ScreenW = 0;
    uint32_t UIRenderer::s_ScreenH = 0;
    uint32_t UIRenderer::s_ViewportW = 1920;
    uint32_t UIRenderer::s_ViewportH = 1080;

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

    void UIRenderer::BuildQuadVertices(const UIDrawCommand& cmd, float* verts) {
        switch (cmd.renderMode) {
        case 0: // overlay - screen space 2D
        case 1: // camera - screen space 2D (shader handles transform)
            verts[0] = cmd.x; verts[1] = cmd.y; verts[2] = cmd.z;
            verts[3] = 0.0f; verts[4] = 0.0f; // uv

            verts[5] = cmd.x + cmd.width; verts[6] = cmd.y; verts[7] = cmd.z;
            verts[8] = 1.0f; verts[9] = 0.0f;

            verts[10] = cmd.x + cmd.width; verts[11] = cmd.y + cmd.height; verts[12] = cmd.z;
            verts[13] = 1.0f; verts[14] = 1.0f;

            verts[15] = cmd.x; verts[16] = cmd.y + cmd.height; verts[17] = cmd.z;
            verts[18] = 0.0f; verts[19] = 1.0f;
            break;

        case 2: // world - unit quad (model matrix scales it)
            verts[0] = 0.0f; verts[1] = 0.0f; verts[2] = 0.0f;
            verts[3] = 0.0f; verts[4] = 0.0f;

            verts[5] = 1.0f; verts[6] = 0.0f; verts[7] = 0.0f;
            verts[8] = 1.0f; verts[9] = 0.0f;

            verts[10] = 1.0f; verts[11] = 1.0f; verts[12] = 0.0f;
            verts[13] = 1.0f; verts[14] = 1.0f;

            verts[15] = 0.0f; verts[16] = 1.0f; verts[17] = 0.0f;
            verts[18] = 0.0f; verts[19] = 1.0f;
            break;
        }
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
            CheckLink(s_CompositeShader, "UI Composite Program");

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

        // sort commands by order (lower renders first)
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

        // get StateCache for efficient pipeline binding
        auto* stateCache = NE::Graphics::GraphicsManager::GetStateCache();

        // render each command based on mode
        for (const auto& cmd : s_Commands) 
        {
            // Use material from the command (set by UIImage component)
            if (!cmd.material) 
            {
                //std::cerr << "[UIRenderer] Warning: Command has no material!" << std::endl;
                continue;
            }

            // bind pipeline (sets shader + GL state)
            if (stateCache)
            {
                stateCache->Bind(cmd.material->GetPipeline());
            }

            auto shader = cmd.material->GetPipeline()->GetSpecification().shader;
            if (!shader) 
            {
                std::cerr << "[UIRenderer] Warning: Material has no shader!" << std::endl;
                continue;
            }

            // build vertex data for this quad
            float verts[20];
            BuildQuadVertices(cmd, verts);

            static bool debugOnce = false;

            if (!debugOnce) {
                std::cout << "\n=== Vertex Data Debug ===" << std::endl;
                std::cout << "Command: x=" << cmd.x << " y=" << cmd.y
                    << " w=" << cmd.width << " h=" << cmd.height << std::endl;

                std::cout << "Screen size (FBO): " << s_ScreenW << "x" << s_ScreenH << std::endl;
                std::cout << "Viewport size (Panel): " << s_ViewportW << "x" << s_ViewportH << std::endl;

                std::cout << "\nVertices:" << std::endl;
                std::cout << "  BL: pos(" << verts[0] << ", " << verts[1] << ", " << verts[2]
                    << ") uv(" << verts[3] << ", " << verts[4] << ")" << std::endl;
                std::cout << "  BR: pos(" << verts[5] << ", " << verts[6] << ", " << verts[7]
                    << ") uv(" << verts[8] << ", " << verts[9] << ")" << std::endl;
                std::cout << "  TR: pos(" << verts[10] << ", " << verts[11] << ", " << verts[12]
                    << ") uv(" << verts[13] << ", " << verts[14] << ")" << std::endl;
                std::cout << "  TL: pos(" << verts[15] << ", " << verts[16] << ", " << verts[17]
                    << ") uv(" << verts[18] << ", " << verts[19] << ")" << std::endl;

                // Calculate what NDC coords shader will produce
                float ndcX_left = (verts[0] / s_ViewportW) * 2.0f - 1.0f;
                float ndcX_right = (verts[5] / s_ViewportW) * 2.0f - 1.0f;
                float ndcY_bottom = 1.0f - (verts[1] / s_ViewportH) * 2.0f;
                float ndcY_top = 1.0f - (verts[11] / s_ViewportH) * 2.0f;

                float ndcWidth = ndcX_right - ndcX_left;
                float ndcHeight = ndcY_bottom - ndcY_top;
                std::cout << "  NDC size: " << ndcWidth << " x " << ndcHeight << std::endl;

                float expectedScreenWidth = ndcWidth * s_ViewportW * 0.5f;
                float expectedScreenHeight = ndcHeight * s_ViewportH * 0.5f;
                std::cout << "  Expected screen size: " << expectedScreenWidth << " x "
                    << expectedScreenHeight << " pixels" << std::endl;

                std::cout << "======================\n" << std::endl;
                debugOnce = true;
            }

            glBindVertexArray(s_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

            // bind material uniforms (empty for UI, but kept for consistency)
            cmd.material->Bind();

            // set UI-specific uniforms
            shader->SetUniformVec4("uColor", cmd.color);

            switch (cmd.renderMode) {
            case 0: // overlay mode
                glDisable(GL_DEPTH_TEST);
                shader->SetUniformVec2("uScreenSize",
                    NE::Math::Vec2((float)s_ViewportW, (float)s_ViewportH));
                break;

            case 1: // camera mode
                glDisable(GL_DEPTH_TEST);
                shader->SetUniformVec2("uScreenSize",
                    NE::Math::Vec2(s_ViewportW, s_ViewportH));
                shader->SetUniformMat4("uView", cmd.viewMatrix);
                shader->SetUniformMat4("uProj", cmd.projMatrix);
                shader->SetUniformFloat("uPlaneDistance", cmd.planeDistance);
                break;

            case 2: // world mode
                glEnable(GL_DEPTH_TEST);
                shader->SetUniformMat4("uModel", cmd.modelMatrix);
                shader->SetUniformMat4("uView", cmd.viewMatrix);
                shader->SetUniformMat4("uProj", cmd.projMatrix);
                break;
            }

            // draw the quad
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }

        // restore state
        glBindVertexArray(0);
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
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

    void UIRenderer::SetViewportSize(uint32_t width, uint32_t height) {
        s_ViewportW = width;
        s_ViewportH = height;
    }
}
