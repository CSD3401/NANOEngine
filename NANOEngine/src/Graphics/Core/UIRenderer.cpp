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

    // simple vertex shader (converts pixel coordinates to NDC)
    static const char* UIVertexShaderSource = R"(#version 460 core
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
    static const char* UIFragmentShaderSource = R"(#version 460 core
    #extension GL_ARB_bindless_texture : require

    in vec2 vUV;
    out vec4 FragColor;

    uniform sampler2D uTex;
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
        // saves screen size
        s_ScreenW = width;
        s_ScreenH = height;

        // UI FBO
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

        // color UI shader
        {
            // compile vertex shader
            unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &UIVertexShaderSource, nullptr);
            glCompileShader(vs);
            CheckCompile(vs, "UI VS");

            // Compile fragment shader
            unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &UIFragmentShaderSource, nullptr);
            glCompileShader(fs);
            CheckCompile(fs, "UI FS");

            // link shader program
            s_Shader = glCreateProgram();
            glAttachShader(s_Shader, vs);
            glAttachShader(s_Shader, fs);
            glLinkProgram(s_Shader);
            CheckLink(s_Shader, "UI Program");

            // clean up shaders (no longer needed after linking)
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

        // unbind
        glBindVertexArray(0);

        // initialize composite shader
        InitCompositeShader();
    }

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
            vec2 flippedUV = vec2(vUV.x, 1.0 - vUV.y);
            vec4 uiColor = texture(uUITexture, flippedUV);
            FragColor = uiColor;
        })";

    void UIRenderer::InitCompositeShader() {
        // UI composite shader
        {
            // compile vertex shader
            unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &compositeVertexShaderSource, nullptr);
            glCompileShader(vs);
            CheckCompile(vs, "UI composite VS");

            // compile fragment shader
            unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &compositeFragmentShaderSource, nullptr);
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

        // save current OpenGL state for 3D scene
        GLboolean depthTestWasEnabled;
        GLboolean blendWasEnabled;
        GLint blendSrc, blendDst;
        glGetBooleanv(GL_DEPTH_TEST, &depthTestWasEnabled);
        glGetBooleanv(GL_BLEND, &blendWasEnabled);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

        // set up openGL state for UI rendering
        glDisable(GL_DEPTH_TEST); // UI is 2D on top
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // use UI shader
        glUseProgram(s_Shader);

        // set screen size uniform
        glUniform2f(glGetUniformLocation(s_Shader, "uScreenSize"), (float)s_ScreenW, (float)s_ScreenH);

        // bind VAO
        // VAO expects 4 verts with layout[x, y, u, v] and an EBO for 2 triangles
        glBindVertexArray(s_VAO);

        // draw each command
        int cmdIndex = 0;
        for (const auto& cmd : s_Commands)
        {
            static bool printed2 = false;
            if (!printed2) {
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

            // upload vertices to VBO
            glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

            // set color uniform
            glUniform4f(glGetUniformLocation(s_Shader, "uColor"), cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w);

            // handle texture if material exists
            //if (cmd.material && !cmd.material->GetTextures().empty()) 
            //{
            //    // get the first texture from the material's texture map
            //    const auto& textures = cmd.material->GetTextures();
            //    auto it = textures.begin();
            //    if (it != textures.end() && it->second) 
            //    {
            //        uint64_t handle = it->second->GetBindlessHandle(); // get bindless handle
            //        if (handle != 0) {
            //            it->second->MakeResident();
            //            glUniformHandleui64ARB(glGetUniformLocation(s_Shader, "uTex"), handle);
            //            glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 1);
            //        }

            //        if (shouldPrint) {
            //            std::cout << "  Bindless handle: " << handle << std::endl;
            //        }
            //    }
            //    else 
            //    {
            //        // material has no valid textures, use solid color
            //        glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 0);

            //        std::cout << "  WARNING: Invalid bindless handle!" << std::endl;
            //        glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 0);
            //    }
            //}
            //else
            {
                // no material or no textures, just solid color
                glUniform1i(glGetUniformLocation(s_Shader, "uUseTexture"), 0);
            }

            // draw the quad
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            cmdIndex++;

            if (!printed2)
            {
                std::cout << "  Drew " << cmdIndex << " UI elements" << std::endl;
                printed2 = true;
            }
        }

        // restore OpenGL state for 3D scene
        glBindVertexArray(0);
        glUseProgram(0);

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

    //void UIRenderer::DrawPickingPass()
    //{
    //    if (!s_PickingFBO) return;
    //    s_PickingFBO->Bind();

    //    glViewport(0, 0, s_ScreenW, s_ScreenH);

    //    // Clear to zero (means “no hit”)
    //    glDisable(GL_DEPTH_TEST);
    //    glDisable(GL_BLEND);
    //    glClearColor(0, 0, 0, 0);
    //    glClear(GL_COLOR_BUFFER_BIT);

    //    glUseProgram(s_PickingShader);
    //    glUniform2f(glGetUniformLocation(s_PickingShader, "uScreenSize"),
    //        (float)s_ScreenW, (float)s_ScreenH);

    //    glBindVertexArray(s_VAO);

    //    for (const auto& cmd : s_Commands) {
    //        float x = cmd.x, y = cmd.y, w = cmd.width, h = cmd.height;
    //        float verts[16] = {
    //            x,     y,     0.f, 0.f,
    //            x + w, y,     1.f, 0.f,
    //            x + w, y + h, 1.f, 1.f,
    //            x,     y + h, 0.f, 1.f
    //        };
    //        glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
    //        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    //        GLint locId = glGetUniformLocation(s_PickingShader, "uEntityId");
    //        glUniform1ui(locId, cmd.entityId);

    //        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    //    }

    //    glBindVertexArray(0);
    //    glUseProgram(0);
    //    s_PickingFBO->Unbind();
    //}

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
