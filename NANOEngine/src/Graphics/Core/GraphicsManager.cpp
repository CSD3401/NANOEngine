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

namespace NANOEngine::Graphics {

    std::vector<ECS::Component::Light*> GraphicsManager::m_lights;

    std::unique_ptr<ICommandBuffer> GraphicsManager::s_CommandBuffer;
    std::unique_ptr<Skybox> GraphicsManager::s_skybox;
    Camera* GraphicsManager::s_ActiveCamera = nullptr;

    void GraphicsManager::Init() {
        s_CommandBuffer = std::make_unique<OpenGL::GLCommandBuffer>();
        s_skybox = std::make_unique<Skybox>();

        std::shared_ptr<Graphics::IShader> basicShader = std::make_shared<Graphics::OpenGL::GLShader>("Library/Shaders/Basic.glsl");
        Graphics::PipelineSpecification pipelineSpec;
        pipelineSpec.shader = basicShader;
        pipelineSpec.CullMode = GL_BACK;
        pipelineSpec.PolygonMode = GL_FILL;
        pipelineSpec.EnableDepthTest = true;
        RegisterPipeline(std::make_shared<Graphics::OpenGL::GLPipeline>(pipelineSpec, "Basic.glsl"));
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

}
