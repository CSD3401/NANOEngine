#include "pch.h"
#include "GLGeometryBuffer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <string>
#include "../Core/Vertex.hpp"
#include "../Core/InstanceData.hpp"
#include "../Interfaces/IVertexBuffer.hpp"
#include "../Interfaces/IIndexBuffer.hpp"

namespace NE::Graphics::OpenGL {

    unsigned int GLGeometryBuffer::s_InstanceVBO = 0;
	unsigned int GLGeometryBuffer::s_ParticleInstanceVBO = 0;

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
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, position));

        // Normal (location = 1)
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, normal));

        // TexCoord (location = 2)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texCoord0));

        // Tangent (location = 3)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, tangents));

        // TexCoord1 (location = 4)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(Vertex, texCoord1));

        // ---- Per-instance attributes ----
        if (s_InstanceVBO == 0) InitInstanceBuffer();
        EnableInstanceLayout(5, 9); // locations 5-8 for mat4, 9 for idRGB, 10-13 for lightmap data

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

    void GLGeometryBuffer::EnableInstanceLayout(int locModel, int locIdRGB)
    {
        // This function should be called once per GLGeometryBuffer on initialization
        // Currently called in constructor

        glBindVertexArray(m_VAO);             // Select this mesh's VAO
        glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO); // Source buffer for instance data

        // InstanceData layout:
        //   64 bytes = mat4 model
        //   12 bytes = vec3 idRGB
        //   4 bytes  = float lightmapEnabled
        //   8 bytes  = vec2 lightmapUvScale
        //   8 bytes  = vec2 lightmapUvOffset
        //   4 bytes  = uint lightmapPageSlot
        //   4 bytes  = uint pad
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

        const size_t idOffset = sizeof(float) * 16;
        const size_t enabledOffset = idOffset + sizeof(float) * 3;
        const size_t scaleOffset = enabledOffset + sizeof(float);
        const size_t offsetOffset = scaleOffset + sizeof(float) * 2;
        const size_t pageSlotOffset = offsetOffset + sizeof(float) * 2;

        glEnableVertexAttribArray(locIdRGB + 1);
        glVertexAttribPointer(
            locIdRGB + 1,
            1,
            GL_FLOAT,
            GL_FALSE,
            stride,
            reinterpret_cast<void*>(enabledOffset)
        );
        glVertexAttribDivisor(locIdRGB + 1, 1);

        glEnableVertexAttribArray(locIdRGB + 2);
        glVertexAttribPointer(
            locIdRGB + 2,
            2,
            GL_FLOAT,
            GL_FALSE,
            stride,
            reinterpret_cast<void*>(scaleOffset)
        );
        glVertexAttribDivisor(locIdRGB + 2, 1);

        glEnableVertexAttribArray(locIdRGB + 3);
        glVertexAttribPointer(
            locIdRGB + 3,
            2,
            GL_FLOAT,
            GL_FALSE,
            stride,
            reinterpret_cast<void*>(offsetOffset)
        );
        glVertexAttribDivisor(locIdRGB + 3, 1);

        glEnableVertexAttribArray(locIdRGB + 4);
        glVertexAttribIPointer(
            locIdRGB + 4,
            1,
            GL_UNSIGNED_INT,
            stride,
            reinterpret_cast<void*>(pageSlotOffset)
        );
        glVertexAttribDivisor(locIdRGB + 4, 1);

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

    void GLGeometryBuffer::InitInstanceBuffer()
    {
        if (s_InstanceVBO != 0) return;

        glGenBuffers(1, &s_InstanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(InstanceData), nullptr, GL_STREAM_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLGeometryBuffer::UpdateInstanceBuffer(const void* instanceData, size_t instanceDataSize)
    {
        if (s_InstanceVBO == 0 || instanceData == nullptr || instanceDataSize == 0) return;

        glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);

        // Check if we need to resize
        GLint currentSize = 0;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &currentSize);
        if (instanceDataSize > static_cast<size_t>(currentSize)) {
            // Need to reallocate
            glBufferData(GL_ARRAY_BUFFER, instanceDataSize, instanceData, GL_STREAM_DRAW);
        }
        else {
            // Can use SubData for better performance
            glBufferSubData(GL_ARRAY_BUFFER, 0, instanceDataSize, instanceData);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLGeometryBuffer::ShutdownInstanceBuffer()
    {
        if (s_InstanceVBO == 0) return;

        glDeleteBuffers(1, &s_InstanceVBO);
        s_InstanceVBO = 0;
    }

    void GLGeometryBuffer::EnableParticleInstanceLayout(int locPosLS, int locSize, int locColor)
    {
        // Ensure the buffer exists
        if (s_ParticleInstanceVBO == 0) InitParticleInstanceBuffer();

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, s_ParticleInstanceVBO);

        // ParticleInstanceData layout:
        //   Vec3 posLS  (3 floats) -> 12 bytes
        //   float size  (1 float)  -> 4 bytes
        //   Vec4 color  (4 floats) -> 16 bytes
        const GLsizei stride = static_cast<GLsizei>(sizeof(NE::Graphics::ParticleInstanceData));
        size_t offset = 0;

        // vec3 i_PosLS
        glEnableVertexAttribArray(locPosLS);
        glVertexAttribPointer(locPosLS, 3, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glVertexAttribDivisor(locPosLS, 1);
        offset += sizeof(float) * 3;

        // float i_Size
        glEnableVertexAttribArray(locSize);
        glVertexAttribPointer(locSize, 1, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glVertexAttribDivisor(locSize, 1);
        offset += sizeof(float) * 1;

        // vec4 i_Color
        glEnableVertexAttribArray(locColor);
        glVertexAttribPointer(locColor, 4, GL_FLOAT, GL_FALSE, stride, (void*)offset);
        glVertexAttribDivisor(locColor, 1);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void GLGeometryBuffer::InitParticleInstanceBuffer()
    {
        if (s_ParticleInstanceVBO != 0) return;

        glGenBuffers(1, &s_ParticleInstanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, s_ParticleInstanceVBO);

        // allocate minimal size; will grow as needed
        glBufferData(GL_ARRAY_BUFFER, sizeof(NE::Graphics::ParticleInstanceData), nullptr, GL_STREAM_DRAW);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLGeometryBuffer::UpdateParticleInstanceBuffer(const void* instanceData, size_t instanceDataSize)
    {
        if (s_ParticleInstanceVBO == 0 || instanceData == nullptr || instanceDataSize == 0) return;

        glBindBuffer(GL_ARRAY_BUFFER, s_ParticleInstanceVBO);

        GLint currentSize = 0;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &currentSize);

        if (instanceDataSize > static_cast<size_t>(currentSize)) {
            glBufferData(GL_ARRAY_BUFFER, instanceDataSize, instanceData, GL_STREAM_DRAW);
        }
        else {
            glBufferSubData(GL_ARRAY_BUFFER, 0, instanceDataSize, instanceData);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void GLGeometryBuffer::ShutdownParticleInstanceBuffer()
    {
        if (s_ParticleInstanceVBO == 0) return;

        glDeleteBuffers(1, &s_ParticleInstanceVBO);
        s_ParticleInstanceVBO = 0;
    }
}
