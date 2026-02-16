#include "UIGeometryBuffer.hpp"
#include <glad/glad.h>

namespace NE::Graphics {

    UIGeometryBuffer::UIGeometryBuffer() {
        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        // Set up vertex attribute layout once (stored in VAO state)
        // UIVertex2 layout: Position(Vec3) + TexCoord(Vec2) + Color(Vec4)
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);

        const GLsizei stride = sizeof(UIVertex2);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(Math::Vec3)));

        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(sizeof(Math::Vec3) + sizeof(Math::Vec2)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    UIGeometryBuffer::~UIGeometryBuffer() {
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
    }

    void UIGeometryBuffer::Upload(const std::vector<UIVertex2>& vertices, const std::vector<uint32_t>& indices) {
        m_indexCount = static_cast<int>(indices.size());

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(UIVertex2),
            vertices.data(),
            GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(uint32_t),
            indices.data(),
            GL_STREAM_DRAW);

        glBindVertexArray(0);
    }

    void UIGeometryBuffer::Bind() const { glBindVertexArray(m_vao); }
    void UIGeometryBuffer::Draw() const { glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr); }
    void UIGeometryBuffer::Unbind() const { glBindVertexArray(0); }

    void UIGeometryBuffer::DrawInstanced(size_t instanceCount) const {
        glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr,
            static_cast<GLsizei>(instanceCount));
    }

} // namespace NE::Graphics
