#pragma once

#include "../Interfaces/IVertexBuffer.hpp"
#include <cstdint>

namespace NANOEngine::Graphics::OpenGL {

    class GLVertexBuffer final : public IVertexBuffer {
    public:
        GLVertexBuffer(const void* data, uint32_t size, size_t stride);
        ~GLVertexBuffer();

        void Bind() const override;
        void SetData(const void* data, uint32_t size) override;
        size_t GetStride() const override;

    private:
        unsigned int m_ID = 0;
        size_t m_Stride;
    };

}