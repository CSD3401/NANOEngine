#include "GLVertexBuffer.hpp"
#include <glad/glad.h>

namespace NE::Graphics::OpenGL {

    GLVertexBuffer::GLVertexBuffer(const void* data, uint32_t size, size_t stride)
        : m_Stride(stride)
    {
        glGenBuffers(1, &m_ID);
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }

    GLVertexBuffer::~GLVertexBuffer() {
        glDeleteBuffers(1, &m_ID);
    }

    void GLVertexBuffer::Bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    }

    void GLVertexBuffer::SetData(const void* data, uint32_t size) {
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }

    size_t GLVertexBuffer::GetStride() const {
        return m_Stride;
    }

}
