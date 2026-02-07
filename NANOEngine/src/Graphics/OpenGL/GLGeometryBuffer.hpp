#pragma once

#include "../Interfaces/IGeometryBuffer.hpp"
#include <memory>

// Forward declarations
namespace NE::Graphics {
    class IVertexBuffer;
    class IIndexBuffer;
}

namespace NE::Graphics::OpenGL {

    class GLGeometryBuffer final : public IGeometryBuffer {
    public:
        GLGeometryBuffer(
            std::shared_ptr<IVertexBuffer> vertexBuffer,
            std::shared_ptr<IIndexBuffer> indexBuffer
        );
        ~GLGeometryBuffer();

        void Bind() const override;
        void Draw() const override;
        void Unbind() const override;
		void EnableInstanceLayout(int locModel, int locIdRGB) override;
		void DrawInstanced(size_t instanceCount) const override;

        static void InitInstanceBuffer();
        static void UpdateInstanceBuffer(const void* instanceData, size_t instanceDataSize);
        static void ShutdownInstanceBuffer();
        static unsigned int GetInstanceVBO() { return s_InstanceVBO; }

        // HACK: Allow overriding VAO for custom vertex layouts (UI rendering)
        void SetVAO(unsigned int vao) { m_VAO = vao; }

    private:
        unsigned int m_VAO = 0;
        std::shared_ptr<IVertexBuffer> m_VertexBuffer;
        std::shared_ptr<IIndexBuffer> m_IndexBuffer;

        static unsigned int s_InstanceVBO;
    };

}