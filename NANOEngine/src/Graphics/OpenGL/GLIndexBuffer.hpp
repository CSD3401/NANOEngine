#pragma once

#include "../Interfaces/IIndexBuffer.hpp"

namespace NE::Graphics::OpenGL {

    class GLIndexBuffer final : public IIndexBuffer {
    public:
        GLIndexBuffer(const uint32_t* indices, size_t count);
        ~GLIndexBuffer();

        void Bind() const override;
        size_t GetCount() const override;

    private:
        unsigned int m_ID = 0;
        size_t m_Count;
    };

}