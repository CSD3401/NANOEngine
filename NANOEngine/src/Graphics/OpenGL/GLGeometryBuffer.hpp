#pragma once

#include "../Interfaces/IGeometryBuffer.hpp"
#include <memory>
#include "../Interfaces/IVertexBuffer.hpp"
#include "../Interfaces/IIndexBuffer.hpp"

namespace NANOEngine::Graphics::OpenGL {

    class GLGeometryBuffer final : public IGeometryBuffer {
    public:
        GLGeometryBuffer(std::shared_ptr<IVertexBuffer> vertexBuffer,
            std::shared_ptr<IIndexBuffer> indexBuffer);
        ~GLGeometryBuffer();

        void Bind() const override;
        void Draw() const override;

    private:
        unsigned int m_VAO = 0;
        std::shared_ptr<IVertexBuffer> m_VertexBuffer;
        std::shared_ptr<IIndexBuffer> m_IndexBuffer;
    };

}