#include "GraphicsManager.hpp"
#include "../OpenGL/GLCommandBuffer.hpp"
#include "../Interfaces/IShader.hpp"
#include "Camera.hpp"
#include "Skybox.hpp"
#include <glad/glad.h>
#include "../../Core/Logger.hpp"

namespace NANOEngine::Graphics {

    std::unique_ptr<ICommandBuffer> GraphicsManager::s_CommandBuffer;
    std::unique_ptr<Skybox> GraphicsManager::s_skybox;
    Camera* GraphicsManager::s_ActiveCamera = nullptr;

    void GraphicsManager::Init() {
        s_CommandBuffer = std::make_unique<OpenGL::GLCommandBuffer>();
        s_skybox = std::make_unique<Skybox>();
    }

    void GraphicsManager::BeginFrame() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(1.f, 1.f, 1.f, 1.f);
        s_CommandBuffer->Begin();
        s_CommandBuffer->BeginRenderPass();
        DrawSkybox();
    }

    void GraphicsManager::DrawSkybox()
    {
        if (s_skybox)
            s_skybox->Draw();
    }

    void GraphicsManager::Submit(const DrawCommand& command) {

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

        // Draw indexed
        //s_CommandBuffer->DrawIndexed(command.mesh->GetIndexCount());
        command.mesh->Draw();

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

}
