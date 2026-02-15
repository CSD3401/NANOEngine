#pragma once
#include "../Interfaces/IGeometryBuffer.hpp"
#include "UIImageMeshGenerator.hpp"
#include <glad/glad.h>
#include <vector>
#include <cstdint>

namespace NE::Graphics {

    // Lightweight geometry buffer for UI rendering.
    // Owns its VAO/VBO/EBO and properly cleans them up,
    // avoiding the double-allocation issue with GLGeometryBuffer + SetVAO hack.
    class UIGeometryBuffer final : public IGeometryBuffer {
    public:
        UIGeometryBuffer() {
            glGenVertexArrays(1, &m_vao);
            glGenBuffers(1, &m_vbo);
            glGenBuffers(1, &m_ebo);
        }

        ~UIGeometryBuffer() override {
            if (m_vao) glDeleteVertexArrays(1, &m_vao);
            if (m_vbo) glDeleteBuffers(1, &m_vbo);
            if (m_ebo) glDeleteBuffers(1, &m_ebo);
        }

        // Upload new vertex/index data (can be called every frame to reuse the buffer)
        void Upload(const std::vector<UIVertex2>& vertices, const std::vector<uint32_t>& indices) {
            m_indexCount = static_cast<GLsizei>(indices.size());

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

            // UIVertex2 layout: Position(Vec3) + TexCoord(Vec2) + Color(Vec4)
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

        void Bind() const override { glBindVertexArray(m_vao); }
        void Draw() const override { glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr); }
        void Unbind() const override { glBindVertexArray(0); }
        void EnableInstanceLayout(int, int) override {} // UI shader has no per-instance attributes
        void DrawInstanced(size_t instanceCount) const override {
            glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr,
                static_cast<GLsizei>(instanceCount));
        }

    private:
        GLuint m_vao = 0;
        GLuint m_vbo = 0;
        GLuint m_ebo = 0;
        GLsizei m_indexCount = 0;
    };

} // namespace NE::Graphics
