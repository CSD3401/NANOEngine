#pragma once

#include "../Interfaces/IFrameBuffer.hpp"

namespace NE::Graphics::OpenGL {

    class GLFrameBuffer final : public IFrameBuffer {
    public:
        enum class FormatMode {
            Standard,
            HDR,
            LDR
        };

        GLFrameBuffer();
        GLFrameBuffer(uint32_t width, uint32_t height);
        ~GLFrameBuffer();

        void CreateAsHDR(uint32_t width, uint32_t height, bool enablePicking, bool enableMiniGBuffer = false);
        void CreateAsLDR(uint32_t width, uint32_t height, bool enablePicking, bool enableMiniGBuffer = false);

        void Bind() const override;
        void Resize(uint32_t width, uint32_t height) override;
		void Clear() override;

		// Enable or disable writing to the picking attachment
        void SetPickingWrite(bool enable) override;

        uint32_t GetColorAttachment() const override { return m_ColorAttachment; }
        uint32_t GetDepthAttachment() const override { return m_DepthAttachment; }
        uint32_t GetNormalAttachment() const override { return m_NormalAttachment; }
        bool HasMiniGBuffer() const override { return m_EnableMiniGBuffer; }
        uint32_t GetWidth() const override { return m_Width; }
        uint32_t GetHeight() const override { return m_Height; }
        uint32_t GetFramebuffer() const override { return m_FBO; }

		// Read pixel data from the picking attachment
		uint32_t ReadPixel(uint32_t x, uint32_t y) override;
        void ReadPixelRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint32_t>& outIds) override;

		// Blit this framebuffer to the default framebuffer (screen)
		void BlitToScreen(int windowWidth, int windowHeight);

        static void Unbind();

    private:
        void Invalidate();
        void Configure(FormatMode mode, uint32_t width, uint32_t height, bool enablePicking, bool enableMiniGBuffer);
        void RebuildAttachments();
        void ApplyDrawBuffers() const;
        void DestroyAttachments();

        uint32_t m_FBO = 0;
		uint32_t m_ColorAttachment = 0; // Color for normal rendering
		uint32_t m_PickingAttachment = 0; // Color for object picking
        uint32_t m_NormalAttachment = 0; // optional mini-gbuffer normal
        uint32_t m_DepthAttachment = 0;

        uint32_t m_Width = 0, m_Height = 0;
        FormatMode m_Mode = FormatMode::Standard;
        bool m_EnablePicking = true;
        bool m_EnableMiniGBuffer = false;
        bool m_PickingWriteEnabled = true;
    };
}
