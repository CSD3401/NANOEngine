#pragma once

#include "../Interfaces/IFrameBuffer.hpp"

namespace NE::Graphics::OpenGL {

    class GLFrameBuffer final : public IFrameBuffer {
    public:
        GLFrameBuffer();
        GLFrameBuffer(uint32_t width, uint32_t height);
        ~GLFrameBuffer();

        void CreateAsHDR(uint32_t width, uint32_t height, bool enablePicking);
        void CreateAsLDR(uint32_t width, uint32_t height, bool enablePicking);

        void Bind() const override;
        void Resize(uint32_t width, uint32_t height) override;
		void Clear() override;

		// Enable or disable writing to the picking attachment
        void SetPickingWrite(bool enable) override;

        uint32_t GetColorAttachment() const override { return m_ColorAttachment; }
        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }

		// Read pixel data from the picking attachment
		uint32_t ReadPixel(uint32_t x, uint32_t y);

        static void Unbind();

    private:
        void Invalidate();

        uint32_t m_FBO = 0;
		uint32_t m_ColorAttachment = 0; // Color for normal rendering
		uint32_t m_PickingAttachment = 0; // Color for object picking
        uint32_t m_RBO = 0; // Depth-stencil

        uint32_t m_Width = 0, m_Height = 0;
    };

}
