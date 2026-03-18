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
		void EnableParticleInstanceLayout(int locPosLS, int locSize, int locColor) override;
		void DrawInstanced(size_t instanceCount) const override;

        static void InitInstanceBuffer();
        static void UpdateInstanceBuffer(const void* instanceData, size_t instanceDataSize);
        static void ShutdownInstanceBuffer();
        static unsigned int GetInstanceVBO() { return s_InstanceVBO; }

        static void InitParticleInstanceBuffer();
        static void UpdateParticleInstanceBuffer(const void* instanceData, size_t instanceDataSize);
        static void ShutdownParticleInstanceBuffer();
		static unsigned int GetParticleInstanceVBO() { return s_ParticleInstanceVBO; }

    private:
        unsigned int m_VAO = 0;
        std::shared_ptr<IVertexBuffer> m_VertexBuffer;
        std::shared_ptr<IIndexBuffer> m_IndexBuffer;

        static unsigned int s_InstanceVBO;
        static unsigned int s_ParticleInstanceVBO;
    };

}