#pragma once

#include "../Interfaces/IFrameBuffer.hpp"

namespace NE::Graphics::OpenGL {

    class GLFrameBuffer final : public IFrameBuffer {
    public:
        GLFrameBuffer(uint32_t width, uint32_t height);
        ~GLFrameBuffer();

        void Bind() const override;
        void Unbind() const override;
        void Resize(uint32_t width, uint32_t height) override;

        uint32_t GetColorAttachment() const override { return m_ColorAttachment; }
        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

    private:
        void Invalidate();

        uint32_t m_FBO = 0;
        uint32_t m_ColorAttachment = 0;
        uint32_t m_RBO = 0; // Renderbuffer for depth-stencil

        uint32_t m_Width = 0, m_Height = 0;
    };

}
