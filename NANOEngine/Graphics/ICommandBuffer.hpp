#pragma once
#include <cstdint>

namespace NANOEngine::Graphics {

    class ICommandBuffer {
    public:
        virtual ~ICommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual void BeginRenderPass() = 0;
        virtual void EndRenderPass() = 0;

        virtual void BindPipeline(/* Pipeline pointer or ID */) = 0;
        virtual void BindVertexBuffer(/* Buffer pointer */) = 0;
        virtual void BindIndexBuffer(/* Buffer pointer */) = 0;
        virtual void BindDescriptorSet(/* Resources like textures, uniforms */) = 0;

        virtual void Draw(uint32_t vertexCount, uint32_t instanceCount = 1) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1) = 0;
    };

}