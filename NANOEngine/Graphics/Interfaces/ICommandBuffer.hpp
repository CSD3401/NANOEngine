#pragma once
#include <cstdint>
#include <memory>

namespace NANOEngine::Graphics {
    class IPipeline;
    class IVertexBuffer;
    class IIndexBuffer;

    class ICommandBuffer {
    public:
        virtual ~ICommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual void BeginRenderPass() = 0;
        virtual void EndRenderPass() = 0;

        virtual void BindPipeline(std::shared_ptr<IPipeline> pipeline) = 0;
        virtual void BindVertexBuffer(const std::shared_ptr<IVertexBuffer>& vertexBuffer) = 0;
        virtual void BindIndexBuffer(const std::shared_ptr<IIndexBuffer>& indexBuffer) = 0;
        virtual void BindDescriptorSet(/* Resources like textures, uniforms */) = 0;

        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) = 0;
    };

}