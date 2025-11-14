#include "GLGeometryBuffer.hpp"
#include <glad/glad.h>
#include "../Core/Vertex.hpp"

namespace NE::Graphics::OpenGL {

    GLGeometryBuffer::GLGeometryBuffer(std::shared_ptr<IVertexBuffer> vb, std::shared_ptr<IIndexBuffer> ib)
        : m_VertexBuffer(vb), m_IndexBuffer(ib)
    {
        glGenVertexArrays(1, &m_VAO);
        glBindVertexArray(m_VAO);

        vb->Bind();
        ib->Bind();

        //glEnableVertexAttribArray(0);
        //glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(vb->GetStride()), (void*)0);

        //glEnableVertexAttribArray(1);
        //glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vb->GetStride(), (void*)(sizeof(float) * 2));


        const GLsizei stride = static_cast<GLsizei>(vb->GetStride());

        // Position (location = 0)
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, Position));

        // Normal (location = 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, Normal));

        // TexCoord (location = 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, TexCoord));

        // Bone IDs (location = 3) - integer attribute
        glEnableVertexAttribArray(3);
        glVertexAttribIPointer(3, 4, GL_INT, stride, (void*)offsetof(Vertex, BoneIDs));

        // Bone Weights (location = 4)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, Weights));

        glBindVertexArray(0);
    }

    GLGeometryBuffer::~GLGeometryBuffer()
    {
    }

    void GLGeometryBuffer::Bind() const {
        glBindVertexArray(m_VAO);
    }

    void GLGeometryBuffer::Draw() const {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_IndexBuffer->GetCount()), GL_UNSIGNED_INT, nullptr);
    }

}