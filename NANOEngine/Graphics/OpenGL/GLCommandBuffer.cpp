#include "GLCommandBuffer.hpp"
#include <glad/glad.h>

namespace NANOEngine::Graphics::OpenGL {

    GLCommandBuffer::GLCommandBuffer() {
        // Constructor if needed
    }

    GLCommandBuffer::~GLCommandBuffer() {
        // Destructor if needed
    }

    void GLCommandBuffer::Begin() {
        // In OpenGL, not much to do
    }

    void GLCommandBuffer::End() {
        // In OpenGL, not much to do
    }

    void GLCommandBuffer::BeginRenderPass() {
        // Bind framebuffer if needed
        // glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    }

    void GLCommandBuffer::EndRenderPass() {
        // Unbind framebuffer if needed
        // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLCommandBuffer::BindPipeline() {
        // glUseProgram(programID);
    }

    void GLCommandBuffer::BindVertexBuffer() {
        // glBindBuffer(GL_ARRAY_BUFFER, vboID);
        // glVertexAttribPointer(...);
    }

    void GLCommandBuffer::BindIndexBuffer() {
        // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboID);
    }

    void GLCommandBuffer::BindDescriptorSet() {
        // Bind textures, uniforms manually
        // glBindTexture(GL_TEXTURE_2D, textureID);
    }

    void GLCommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount) {
        if (instanceCount == 1)
            glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        else
            glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
    }

    void GLCommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount) {
        if (instanceCount == 1)
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        else
            glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr, instanceCount);
    }

}
