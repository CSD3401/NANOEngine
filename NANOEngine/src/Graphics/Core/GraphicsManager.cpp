#include "GraphicsManager.hpp"
#include "../OpenGL/GLCommandBuffer.hpp"
#include "../Interfaces/IShader.hpp"
#include "Camera.hpp"
#include "Skybox.hpp"
#include <glad/glad.h>
#include "../../Core/Logger.hpp"
#include "../../Core/Profiler.hpp"
#include "../Interfaces/IFrameBuffer.hpp"
#include "../../ECS/Components/Light.hpp"
#include "../OpenGL/GLShader.hpp"
#include "../OpenGL/GLPipeline.hpp"
#include "../../AssetManager.hpp"
#include "../Core/Primitives.hpp"
#include <GL/gl.h> // Add this include for OpenGL functions like glBegin, glEnd, etc.


namespace NANOEngine::Graphics {
    void InitDebugLines();

    std::vector<ECS::Component::Light*> GraphicsManager::m_lights;

    std::unique_ptr<ICommandBuffer> GraphicsManager::s_CommandBuffer;
    std::unique_ptr<Skybox> GraphicsManager::s_skybox;
    Camera* GraphicsManager::s_ActiveCamera = nullptr;

    std::vector<DebugLine> GraphicsManager::s_DebugLines;

    void GraphicsManager::Init() {
        s_CommandBuffer = std::make_unique<OpenGL::GLCommandBuffer>();
        s_skybox = std::make_unique<Skybox>();

        // Load Primitives
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreateCube(), "Cube");
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreatePlane(), "Plane");
        Asset::AssetManager::GetInstance().AddToMap<Graphics::Model>(CreateCylinder(), "Cylinder");

        // temp
        InitDebugLines();
    }

    void GraphicsManager::BeginFrame() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(1.f, 1.f, 1.f, 1.f);
        s_CommandBuffer->Begin();
        s_CommandBuffer->BeginRenderPass();
    }

    void GraphicsManager::DrawSkybox()
    {
        if (s_skybox)
            s_skybox->Draw();
    }

    void GraphicsManager::Submit(const DrawCommand& command) {
        NE_PROFILE_FUNCTION();
        // Bind the pipeline (shader program + GL state)
        s_CommandBuffer->BindPipeline(command.material->GetPipeline());

        // Bind the vertex/index buffers
        command.mesh->Bind();

        // Bind material (textures, uniforms, etc.)
        command.material->Bind();

        // Upload transform matrix to shader
        auto shader = command.material->GetPipeline()->GetSpecification().shader;
        shader->SetUniformMat4("u_Model", command.transform);
        shader->SetUniformMat4("u_View", s_ActiveCamera->GetViewMatrix());
        shader->SetUniformMat4("u_Projection", s_ActiveCamera->GetProjectionMatrix());
        shader->SetUniformVec3("u_CameraPos", s_ActiveCamera->GetPosition());

        shader->SetUniformInt("u_numLights", static_cast<int>(m_lights.size()));
        for (size_t i = 0; i < m_lights.size(); ++i) {
            const auto* light = m_lights[i];
            std::string base = "u_lights[" + std::to_string(i) + "]";
            shader->SetUniformInt(base + ".type", light->type);
            shader->SetUniformVec3(base + ".position", light->position);
            shader->SetUniformVec3(base + ".direction", light->direction);
            shader->SetUniformVec3(base + ".color", light->color);
            shader->SetUniformFloat(base + ".intensity", light->intensity);
            shader->SetUniformFloat(base + ".innerCutoff", light->innerCutoff);
            shader->SetUniformFloat(base + ".outerCutoff", light->outerCutoff);
            shader->SetUniformFloat(base + ".constant", light->constant);
            shader->SetUniformFloat(base + ".linear", light->linear);
            shader->SetUniformFloat(base + ".quadratic", light->quadratic);
        }

        // Draw indexed
        //s_CommandBuffer->DrawIndexed(command.mesh->GetIndexCount());
        command.mesh->Draw();

        glBindVertexArray(0);

        //GLenum err;
        //while ((err = glGetError()) != GL_NO_ERROR) {
        //    std::cout << "OpenGL Error: " << std::hex << err << std::endl;
        //}

    }

    void GraphicsManager::EndFrame() {
        s_CommandBuffer->EndRenderPass();
        s_CommandBuffer->End();
    }

    void GraphicsManager::Shutdown() {
        s_skybox.reset();
        s_CommandBuffer.reset();
    }

    void GraphicsManager::SetCamera(Camera* cam) {
        s_ActiveCamera = cam;
    }

    uint32_t GraphicsManager::ReadPixel(IFrameBuffer* framebuffer, uint32_t x, uint32_t y) {
        framebuffer->Bind();
        uint8_t data[4] = { 0, 0, 0, 0 };
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
        framebuffer->Unbind();

        uint32_t id = data[0] | (data[1] << 8) | (data[2] << 16);
        return id;
    }

    const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    out vec3 color;
    void main() {
        gl_Position = vec4(aPos, 1.0);
        color = aColor;
    }
)";

    const char* fragmentShaderSource = R"(
    #version 330 core
    in vec3 color;
    out vec4 FragColor;
    void main() {
        FragColor = vec4(color, 1.0);
    }
)";

    GLuint debugShaderProgram, debugVAO, debugVBO;

    void InitDebugLines() {
        // Compile shaders
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);

        debugShaderProgram = glCreateProgram();
        glAttachShader(debugShaderProgram, vertexShader);
        glAttachShader(debugShaderProgram, fragmentShader);
        glLinkProgram(debugShaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Create VAO & VBO
        glGenVertexArrays(1, &debugVAO);
        glGenBuffers(1, &debugVBO);

        glBindVertexArray(debugVAO);
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);

        // Position attribute (location = 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Color attribute (location = 1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void GraphicsManager::AddDebugLine(const Math::Vec3& from, const Math::Vec3& to, const Math::Vec3& color) {
        s_DebugLines.push_back({ from, to, color });
    }

    void GraphicsManager::DrawDebugLines() {
        if (s_DebugLines.empty()) return;

        // Prepare vertex data (interleaved positions + colors)
        std::vector<float> vertices;
        for (const auto& line : s_DebugLines) {
            vertices.push_back(line.from.x);
            vertices.push_back(line.from.y);
            vertices.push_back(line.from.z);
            vertices.push_back(line.color.x);
            vertices.push_back(line.color.y);
            vertices.push_back(line.color.z);

            vertices.push_back(line.to.x);
            vertices.push_back(line.to.y);
            vertices.push_back(line.to.z);
            vertices.push_back(line.color.x);
            vertices.push_back(line.color.y);
            vertices.push_back(line.color.z);
        }

        // Upload data to GPU
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);

        // Draw
        glUseProgram(debugShaderProgram);
        glBindVertexArray(debugVAO);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_DebugLines.size() * 2)); // 2 vertices per line

        s_DebugLines.clear();
    }

}
