#include "GLGeometryBuffer.hpp"
#include "../Core/Vertex.hpp"
#include "../Core/InstanceData.hpp"
#include <glad/glad.h>

namespace NE::Graphics::OpenGL {

    unsigned int GLGeometryBuffer::s_InstanceVBO = 0;

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

        // ---- Per-vertex attributes ----

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

        // ---- Per-instance attributes ----
        if (s_InstanceVBO == 0) InitInstanceBuffer();
		EnableInstanceLayout(5, 9); // locations 5-8 for mat4, 9 for vec3

        glBindVertexArray(0);
    }

    GLGeometryBuffer::~GLGeometryBuffer()
    {
    }

    void GLGeometryBuffer::Bind() const 
    {
        glBindVertexArray(m_VAO);
    }

    void GLGeometryBuffer::Draw() const 
    {
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_IndexBuffer->GetCount()), GL_UNSIGNED_INT, nullptr);
    }

    void GLGeometryBuffer::Unbind() const 
    {
        glBindVertexArray(0);
	}

    void GLGeometryBuffer::InitInstanceBuffer() 
    {
        if (s_InstanceVBO != 0) return;

        glGenBuffers(1, &s_InstanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

    void GLGeometryBuffer::UpdateInstanceBuffer(const void* instanceData, size_t instanceDataSize) 
    {
        if (s_InstanceVBO == 0 || instanceData == nullptr || instanceDataSize == 0) return;
		
        glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instanceDataSize, instanceData, GL_STREAM_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

    void GLGeometryBuffer::ShutdownInstanceBuffer() 
    {
        if (s_InstanceVBO == 0) return;

        glDeleteBuffers(1, &s_InstanceVBO);
        s_InstanceVBO = 0;
	}

    void GLGeometryBuffer::EnableInstanceLayout(int locModel, int locIdRGB)
    {
		// This function should be called once per GLGeometryBuffer on initialization
		// Currently called in constructor

        glBindVertexArray(m_VAO);             // Select this mesh's VAO
        glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO); // Source buffer for instance data

        // InstanceDataRGB layout:
        //   64 bytes = mat4 model
        //   12 bytes = vec3 idRGB
        //   4 bytes  = float pad
		const GLsizei stride = static_cast<GLsizei>(sizeof(InstanceData));
        size_t offset = 0;

        // ---- 1) mat4 i_Model (4 vec4s) at locations locModel to locModel+3 ----
        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(locModel + i);

            // Read a vec4 at (offset + 16*i)
            glVertexAttribPointer(
                locModel + i,        // attribute index
                4,                   // 4 floats
                GL_FLOAT,            // type
                GL_FALSE,            // normalize
                stride,              // size of one instance
                (void*)offset        // byte offset inside struct
            );

			// This attribute advances once per instance
            glVertexAttribDivisor(locModel + i, 1);

            offset += sizeof(float) * 4; // move to next column (16 bytes)
        }

        // ---- 2) vec3 i_IDRGB at location locIdRGB ----
        glEnableVertexAttribArray(locIdRGB);
        glVertexAttribPointer(
            locIdRGB,
            3,               // vec3
            GL_FLOAT,
            GL_FALSE,
            stride,
            (void*)(sizeof(float) * 16) // mat4 = 16 floats = 64 bytes
        );
        glVertexAttribDivisor(locIdRGB, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void GLGeometryBuffer::DrawInstanced(size_t instanceCount) const 
    {
        glBindVertexArray(m_VAO);
        glDrawElementsInstanced(
            GL_TRIANGLES, 
            static_cast<GLsizei>(m_IndexBuffer->GetCount()),
            GL_UNSIGNED_INT, 
            nullptr, 
            static_cast<GLsizei>(instanceCount)
        );
		glBindVertexArray(0);
	}
}