#include "GLCommandBuffer.hpp"
#include <glad/glad.h>
#include "GLPipeline.hpp"
#include "GLVertexBuffer.hpp"
#include "GLIndexBuffer.hpp"

namespace NE::Graphics::OpenGL {

    GLCommandBuffer::GLCommandBuffer() {

    }

    GLCommandBuffer::~GLCommandBuffer() {

    }

    void GLCommandBuffer::Begin() {
        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClearDepth(1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void GLCommandBuffer::End() {
        // Empty for OpenGL
    }

    void GLCommandBuffer::BeginRenderPass() {
        // Bind framebuffer if needed
        // glBindFramebuffer(GL_FRAMEBUFFER, framebufferID);
    }

    void GLCommandBuffer::EndRenderPass() {
        // Unbind framebuffer if needed
        // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLCommandBuffer::BindPipeline(std::shared_ptr<IPipeline> pipeline) {
        // glUseProgram(programID);
        pipeline->Bind();
    }

    void GLCommandBuffer::BindVertexBuffer(const std::shared_ptr<IVertexBuffer>& vertexBuffer) {
        // glBindBuffer(GL_ARRAY_BUFFER, vboID);
        // glVertexAttribPointer(...);
        vertexBuffer->Bind();
    }

    void GLCommandBuffer::BindIndexBuffer(const std::shared_ptr<IIndexBuffer>& indexBuffer) {
        // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboID);
        indexBuffer->Bind();
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
        // Not used, currently using IGeometryBuffer for indexed draws
        if (instanceCount == 1)
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        else
            glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr, instanceCount);
    }

}
