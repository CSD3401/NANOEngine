#pragma once

#include "../Interfaces/ICommandBuffer.hpp"

namespace NE::Graphics::OpenGL {
    
    class GLCommandBuffer final : public ICommandBuffer {
    public:
        GLCommandBuffer();
        ~GLCommandBuffer();

        void Begin() override;
        void End() override;

        void BeginRenderPass() override;
        void EndRenderPass() override;

        void BindPipeline(std::shared_ptr<IPipeline> pipeline) override;
        void BindVertexBuffer(const std::shared_ptr<IVertexBuffer>& vertexBuffer) override;
        void BindIndexBuffer(const std::shared_ptr<IIndexBuffer>& indexBuffer) override;
        void BindDescriptorSet(/* textures, uniforms */) override;

        void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) override;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) override;
    };

}