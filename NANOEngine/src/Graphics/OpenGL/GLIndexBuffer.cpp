#include "pch.h"
#include "GLIndexBuffer.hpp"
#include <glad/glad.h>

namespace NE::Graphics::OpenGL {

    GLIndexBuffer::GLIndexBuffer(const uint32_t* indices, size_t count)
        : m_Count(count)
    {
        glGenBuffers(1, &m_ID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
    }

    GLIndexBuffer::~GLIndexBuffer() {
        glDeleteBuffers(1, &m_ID);
    }

    void GLIndexBuffer::Bind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
    }

    size_t GLIndexBuffer::GetCount() const {
        return m_Count;
    }

}
