#pragma once

#include "../ICommandBuffer.hpp"

namespace NANOEngine::Graphics::OpenGL {

    class GLCommandBuffer final : public ICommandBuffer {
    public:
        GLCommandBuffer();
        ~GLCommandBuffer();

        void Begin() override;
        void End() override;

        void BeginRenderPass() override;
        void EndRenderPass() override;

        void BindPipeline(/* maybe a pointer or ID later */) override;
        void BindVertexBuffer(/* pointer to buffer */) override;
        void BindIndexBuffer(/* pointer to buffer */) override;
        void BindDescriptorSet(/* textures, uniforms */) override;

        void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) override;
    };

}