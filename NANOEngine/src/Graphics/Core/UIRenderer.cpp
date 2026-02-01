#include "UIRenderer.hpp"
#include "../OpenGL/GLFrameBuffer.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../src/Graphics/Core/PipelineCache.hpp"
#include "../src/ResourceManagement/ResourceManager.hpp"
#include "../src/Graphics/Core/GraphicsManager.hpp"
#include "UIImageMeshGenerator.hpp"
#include "../OpenGL/GLStateCache.hpp"
#include "../../Core/Logger.hpp"
#include <iostream>
#include <algorithm>

namespace NE::Graphics {

    std::vector<UIDrawCommand> UIRenderer::s_Commands;
    RenderViewHandle UIRenderer::s_UIViewHandle;
    RenderViewManager* UIRenderer::s_RenderViewManager; // pointer to GraphicsManager's instance
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
    // overlay mode shader (screen space - pixel coordinates)
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aUV;

    out vec2 vUV;

    uniform vec2 uScreenSize;

    void main() {
        // Convert pixel coordinates to NDC (-1 to 1)
        float ndcX = (aPos.x / uScreenSize.x) * 2.0 - 1.0;
        float ndcY = 1.0 - (aPos.y / uScreenSize.y) * 2.0;  // Flip Y (top-left origin)
    
        gl_Position = vec4(ndcX, ndcY, aPos.z, 1.0);
        vUV = aUV;
    })";

    static const char* UIFragmentShaderSource = R"(#version 460 core
    #extension GL_ARB_bindless_texture : require
    in vec2 vUV;
    out vec4 FragColor;

    uniform vec4 uColor;
    uniform int uHasTexture;
    layout(bindless_sampler) uniform sampler2D uTexture;

    void main() {
        if (uHasTexture != 0) {
            vec4 texColor = texture(uTexture, vUV);
            FragColor = texColor * uColor;  // Texture tinted by color
        } else {
            FragColor = uColor;  // Solid color only
        }
    })";

    void UIRenderer::Init(uint32_t width, uint32_t height, RenderViewManager* renderViewManager) {
        // saves screen size
        s_ScreenW = width;
        s_ScreenH = height;
        s_RenderViewManager = renderViewManager;

        // Create UI render view (no picking needed for UI)
        s_UIViewHandle = s_RenderViewManager->Create(width, height, false);

        // color UI shader
        // compile overlay shader
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

            glDeleteShader(vs);
            glDeleteShader(fs);
        }

        // generate handles for VAO, VBO, EBO
        glGenVertexArrays(1, &s_VAO);
        glGenBuffers(1, &s_VBO);
        glGenBuffers(1, &s_EBO);

        //// for testing
        //{
        //    // setup VBO (4 vertices, each with pos(2) + uv(2) = 4 floats)
        //    glBindVertexArray(s_VAO);
        //    glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
        //    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 5, nullptr, GL_DYNAMIC_DRAW);

        //    // position attribute (location 0) (2D for overlay/camera, 3D for world)
        //    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        //    glEnableVertexAttribArray(0);

        //    // UV attribute (location 1)
        //    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        //    glEnableVertexAttribArray(1);
        //}

        // with world space support 
        {
            // setup vbo for ui vertices
            glBindVertexArray(s_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, s_VBO);

            // allocate buffer for dynamic vertex data
            glBufferData(GL_ARRAY_BUFFER, sizeof(UIVertex) * 1000, nullptr, GL_DYNAMIC_DRAW);

            // Position (location 0)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)0);
            glEnableVertexAttribArray(0);

            // UV (location 1)
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            // Color (location 2)
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(UIVertex), (void*)(5 * sizeof(float)));
            glEnableVertexAttribArray(2);
        }

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
        case 0: // overlay - screen space 2d (pixel coordinates)
        case 1: // camera - screen space 2d (shader handles transform)
        {
            // format: x, y, z, u, v, r, g, b, a (9 floats per vertex)
            // bottom-left
            verts[0] = cmd.x;
            verts[1] = cmd.y;
            verts[2] = cmd.z;
            verts[3] = 0.0f; // u
            verts[4] = 1.0f; // v (flip V for OpenGL)
            verts[5] = cmd.color.x; // r
            verts[6] = cmd.color.y; // g
            verts[7] = cmd.color.z; // b
            verts[8] = cmd.color.w; // a

            // bottom-right
            verts[9] = cmd.x + cmd.width;
            verts[10] = cmd.y;
            verts[11] = cmd.z;
            verts[12] = 1.0f;
            verts[13] = 1.0f;
            verts[14] = cmd.color.x;
            verts[15] = cmd.color.y;
            verts[16] = cmd.color.z;
            verts[17] = cmd.color.w;

            // top-right
            verts[18] = cmd.x + cmd.width;
            verts[19] = cmd.y + cmd.height;
            verts[20] = cmd.z;
            verts[21] = 1.0f;
            verts[22] = 0.0f;
            verts[23] = cmd.color.x;
            verts[24] = cmd.color.y;
            verts[25] = cmd.color.z;
            verts[26] = cmd.color.w;

            // top-left
            verts[27] = cmd.x;
            verts[28] = cmd.y + cmd.height;
            verts[29] = cmd.z;
            verts[30] = 0.0f;
            verts[31] = 0.0f;
            verts[32] = cmd.color.x;
            verts[33] = cmd.color.y;
            verts[34] = cmd.color.z;
            verts[35] = cmd.color.w;
            break;
        }

        case 2: // world - unit quad (0-1, model matrix scales it)
        {
            // unit quad for world space
            // bottom-left
            verts[0] = 0.0f;
            verts[1] = 0.0f;
            verts[2] = 0.0f;
            verts[3] = 0.0f;
            verts[4] = 1.0f;
            verts[5] = cmd.color.x;
            verts[6] = cmd.color.y;
            verts[7] = cmd.color.z;
            verts[8] = cmd.color.w;

            // bottom-right
            verts[9] = 1.0f;
            verts[10] = 0.0f;
            verts[11] = 0.0f;
            verts[12] = 1.0f;
            verts[13] = 1.0f;
            verts[14] = cmd.color.x;
            verts[15] = cmd.color.y;
            verts[16] = cmd.color.z;
            verts[17] = cmd.color.w;

            // top-right
            verts[18] = 1.0f;
            verts[19] = 1.0f;
            verts[20] = 0.0f;
            verts[21] = 1.0f;
            verts[22] = 0.0f;
            verts[23] = cmd.color.x;
            verts[24] = cmd.color.y;
            verts[25] = cmd.color.z;
            verts[26] = cmd.color.w;

            // top-left
            verts[27] = 0.0f;
            verts[28] = 1.0f;
            verts[29] = 0.0f;
            verts[30] = 0.0f;
            verts[31] = 0.0f;
            verts[32] = cmd.color.x;
            verts[33] = cmd.color.y;
            verts[34] = cmd.color.z;
            verts[35] = cmd.color.w;
            break;
        }
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
            -1.0f, -1.0f,  0.0f, 0.0f,  // Bottom-left
            1.0f, -1.0f,  1.0f, 0.0f,  // Bottom-right
            -1.0f,  1.0f,  0.0f, 1.0f,  // Top-left
            -1.0f,  1.0f,  0.0f, 1.0f,  // Top-left
            1.0f, -1.0f,  1.0f, 0.0f,  // Bottom-right
            1.0f,  1.0f,  1.0f, 1.0f   // Top-right
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

    void UIRenderer::Submit(const UIDrawCommand& cmd) {
        s_Commands.push_back(cmd);
    }

    void UIRenderer::ClearCommands() {
        s_Commands.clear();
    }

    void UIRenderer::BeginFrame() {
        // prepare UI render target before drawing
        s_RenderViewManager->Bind(s_UIViewHandle);

        // set viewport to match UI framebuffer size
        glViewport(0, 0, s_ScreenW, s_ScreenH);

        // clear the framebuffer
        //glClearColor(1, 0, 1, 1); // magenta - for debug
        glClearColor(0, 0, 0, 0); // black transparent
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void UIRenderer::EndFrame() {
        s_RenderViewManager->Unbind();
    }

    void UIRenderer::DrawTestQuad() {
        s_RenderViewManager->Bind(s_UIViewHandle);

        static bool print = false;
        if (!print)
        {
            GLint currentFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
            std::cout << "Current FBO: " << currentFBO << std::endl;
            std::cout << "[DrawTestQuad] Screen size: " << s_ScreenW << "x" << s_ScreenH << std::endl;
            print = true;
        }

        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(s_Shader);
        glUniform2f(glGetUniformLocation(s_Shader, "uScreenSize"), (float)s_ScreenW, (float)s_ScreenH);
        glBindVertexArray(s_VAO);

        // Center screen square (assuming 1920x1080)
        float x = s_ScreenW / 2.f - 100.f;
        float y = s_ScreenH / 2.f - 100.f;
        float testVerts[20] = {
            x,       y,       0.f, 0.f, 0.f,
            x + 200, y,       0.f, 1.f, 0.f,
            x + 200, y + 200, 0.f, 1.f, 1.f,
            x,       y + 200, 0.f, 0.f, 1.f
        };

        glBindBuffer(GL_ARRAY_BUFFER, s_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(testVerts), testVerts);

        glUniform4f(glGetUniformLocation(s_Shader, "uColor"), 0.f, 1.f, 0.f, 1.f); // green
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glBindVertexArray(0);
        glUseProgram(0);

        s_RenderViewManager->Unbind();
    }

    void UIRenderer::DrawUIFrame() {
        //// DEBUG: Log command count
        //static int dbgFrame = 0;
        //if (dbgFrame++ % 120 == 0) {
        //    std::cout << "[UIRenderer::DrawUIFrame] Total commands: " << s_Commands.size() << std::endl;
        //}
        //if (s_Commands.empty()) return;

        // filter to only overlay mode (rendermode 0)
        std::vector<UIDrawCommand> overlayCommands;
        for (const auto& cmd : s_Commands)
        {
            if (cmd.renderMode == 0)  // Overlay only
                overlayCommands.push_back(cmd);
        }

        if (overlayCommands.empty()) return;

        //// DEBUG: Log overlay command count
        //static int frameCount = 0;
        //if (frameCount++ % 60 == 0) {  // Log every 60 frames
        //    std::cout << "[UIRenderer] Drawing " << overlayCommands.size() << " overlay commands" << std::endl;
        //}

        // sort overlay commands by order
        std::sort(overlayCommands.begin(), overlayCommands.end(),
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
        glDisable(GL_DEPTH_TEST);

        // render each command based on mode
        for (const auto& cmd : overlayCommands)
        {
            bool usingFallbackShader = false;
            std::shared_ptr<IShader> shader = nullptr;

            if (cmd.material)
            {
                auto pipeline = cmd.material->GetPipeline();
                if (pipeline)
                {
                    shader = pipeline->GetSpecification().shader;
                }
            }

            // Fallback to built-in shader if no material or shader
            if (!shader)
            {
                glUseProgram(s_Shader);
                usingFallbackShader = true;
                //// DEBUG: Log fallback shader usage
                //static bool loggedOnce = false;
                //if (!loggedOnce) {
                //    std::cout << "[UIRenderer] Using fallback shader for entity " << cmd.entityId
                //              << " at (" << cmd.x << "," << cmd.y << ") size " << cmd.width << "x" << cmd.height
                //              << " color (" << cmd.color.x << "," << cmd.color.y << "," << cmd.color.z << "," << cmd.color.w << ")" << std::endl;
                //    loggedOnce = true;
                //}
            }
            else
            {
                shader->Bind();
                cmd.material->Bind();
            }

            glBindVertexArray(s_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, s_VBO);

            // handle custom vertices (for filled/sliced/tiled images)
            if (cmd.useCustomVertices && !cmd.vertices.empty())
            {
                // upload custom vertex data
                glBufferData(GL_ARRAY_BUFFER,
                    cmd.vertices.size() * sizeof(UIVertex),
                    cmd.vertices.data(),
                    GL_DYNAMIC_DRAW);
            }
            else
            {
                // build standard quad
                float verts[36];
                BuildQuadVertices(cmd, verts);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            }

            // set uniforms
            if (usingFallbackShader)
            {
                // Use built-in shader uniforms
                glUniform4f(glGetUniformLocation(s_Shader, "uColor"),
                    cmd.color.x, cmd.color.y, cmd.color.z, cmd.color.w);
                glUniform2f(glGetUniformLocation(s_Shader, "uScreenSize"),
                    (float)s_ScreenW, (float)s_ScreenH);

                // Set texture if available
                if (cmd.bindlessTextureHandle != 0) {
                    glUniformHandleui64ARB(glGetUniformLocation(s_Shader, "uTexture"), cmd.bindlessTextureHandle);
                    glUniform1i(glGetUniformLocation(s_Shader, "uHasTexture"), 1);
                } else {
                    glUniform1i(glGetUniformLocation(s_Shader, "uHasTexture"), 0);
                }
            }
            else
            {
                if (cmd.bindlessTextureHandle != 0)
                {
                    shader->SetUniformHandle("u_BaseMap", cmd.bindlessTextureHandle);  // Bindless!
                    shader->SetUniformInt("u_HasBaseMap", 1);
                }
                else
                {
                    shader->SetUniformInt("u_HasBaseMap", 0);
                }

                shader->SetUniformVec4("uColor", cmd.color);
                shader->SetUniformVec2("uScreenSize", NE::Math::Vec2((float)s_ScreenW, (float)s_ScreenH));
            }

            // draw
            if (cmd.useCustomVertices && !cmd.vertices.empty())
            {
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(cmd.vertices.size()));
            }
            else
            {
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }

        // restore state
        glBindVertexArray(0);
        if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (!blend) glDisable(GL_BLEND); else glBlendFunc(blendSrc, blendDst);
        if (cullFace) glEnable(GL_CULL_FACE);
    }

    void UIRenderer::Draw3DUIFrame(RenderViewHandle targetView) {

        // filter to camera mode (1) and world space (2) only
        std::vector<UIDrawCommand> commands;
        for (const auto& cmd : s_Commands)
        {
            if (cmd.renderMode == 1 || cmd.renderMode == 2)
                commands.push_back(cmd);
        }

        if (commands.empty()) return;

        // Sort by render mode first (camera before world), then by order
        std::sort(commands.begin(), commands.end(),
            [](const UIDrawCommand& a, const UIDrawCommand& b) {
                // Camera mode (1) renders before world space (2)
                if (a.renderMode != b.renderMode) {
                    return a.renderMode < b.renderMode;
                }
                // Same mode: sort by order
                return a.order < b.order;
            });

        // Bind target using RenderViewManager
        s_RenderViewManager->Bind(targetView);

        glViewport(0, 0, s_ScreenW, s_ScreenH);

        // Save current OpenGL state
        GLboolean depthTest, blend, cullFace;
        GLint blendSrc, blendDst;
        glGetBooleanv(GL_DEPTH_TEST, &depthTest);
        glGetBooleanv(GL_BLEND, &blend);
        glGetBooleanv(GL_CULL_FACE, &cullFace);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrc);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDst);

        // setup UI rendering state for world space
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);

        // Render each world space UI command
        for (const auto& cmd : commands)
        {
            if (!cmd.material) continue;

            auto pipeline = cmd.material->GetPipeline();
            if (!pipeline) continue;

            auto shader = pipeline->GetSpecification().shader;
            if (!shader) continue;

            shader->Bind();
            cmd.material->Bind();

            glBindVertexArray(s_VAO);
            glBindBuffer(GL_ARRAY_BUFFER, s_VBO);

            // Handle custom vertices (for filled/sliced/tiled images)
            if (cmd.useCustomVertices && !cmd.vertices.empty())
            {
                glBufferData(GL_ARRAY_BUFFER,
                    cmd.vertices.size() * sizeof(UIVertex),
                    cmd.vertices.data(),
                    GL_DYNAMIC_DRAW);
            }
            else
            {
                float verts[36];
                BuildQuadVertices(cmd, verts);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            }

            // set world space uniforms
            if (cmd.bindlessTextureHandle != 0)
            {
                shader->SetUniformHandle("u_BaseMap", cmd.bindlessTextureHandle);  // Bindless!
                shader->SetUniformInt("u_HasBaseMap", 1);
            }
            else
            {
                shader->SetUniformInt("u_HasBaseMap", 0);
            }
            shader->SetUniformVec4("uColor", cmd.color);

            // set uniforms based on render mode
            if (cmd.renderMode == 1)  // camera mode
            {
                shader->SetUniformVec2("uScreenSize", NE::Math::Vec2((float)s_ScreenW, (float)s_ScreenH));
                shader->SetUniformMat4("uProj", cmd.projMatrix);
                shader->SetUniformFloat("uPlaneDistance", cmd.planeDistance);
            }
            else if (cmd.renderMode == 2)  // world space
            {
                shader->SetUniformMat4("uModel", cmd.modelMatrix);
                shader->SetUniformMat4("uView", cmd.viewMatrix);
                shader->SetUniformMat4("uProj", cmd.projMatrix);
            }

            // draw
            if (cmd.useCustomVertices && !cmd.vertices.empty())
            {
                glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(cmd.vertices.size()));
            }
            else
            {
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }
        }

        // Restore state
        glBindVertexArray(0);
        if (depthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (!blend) glDisable(GL_BLEND); else glBlendFunc(blendSrc, blendDst);
        if (cullFace) glEnable(GL_CULL_FACE);

        s_RenderViewManager->Unbind();
    }

    void UIRenderer::Composite(RenderViewHandle targetView) {

        // only need to composite overlay and camera UI (modes 0 and 1)
        // world space UI (mode 2) is already in the scene FBO from Draw3DUIFrame
        bool hasScreenSpaceUI = false;
        for (const auto& cmd : s_Commands)
        {
            if (cmd.renderMode < 2) // 0 or 1
            {
                hasScreenSpaceUI = true;
                break;
            }
        }

        if (!hasScreenSpaceUI) return; // nothing to composite

        // Bind target using RenderViewManager
        s_RenderViewManager->Bind(targetView);

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
        glDisable(GL_DEPTH_TEST); // screen space ui has no depth
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        // use composite shader
        glUseProgram(s_CompositeShader);

        // bind UI framebuffer texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_RenderViewManager->GetFramebuffer(s_UIViewHandle)->GetColorAttachment()); // FBO�s color attachment is a texture containing your rendered UI
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

        s_RenderViewManager->Unbind();
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

        if (s_Shader) {
            glDeleteProgram(s_Shader);
            s_Shader = 0;
        }

        if (s_CompositeShader)
        {
            glDeleteProgram(s_CompositeShader);
            s_CompositeShader = 0;
        }

        // clear containers
        s_Commands.clear();
        s_UIViewHandle = {};  // Reset to invalid/default handle
        s_RenderViewManager = nullptr;
    }
}