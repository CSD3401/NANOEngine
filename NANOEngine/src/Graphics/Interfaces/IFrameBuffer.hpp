#pragma once
#include <cstdint>
#include <vector>

namespace NE::Graphics {

    class IFrameBuffer {
    public:
        virtual ~IFrameBuffer() = default;

        virtual void Bind() const = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

		virtual void Clear() = 0;

        virtual void SetPickingWrite(bool enable) = 0;

        virtual uint32_t GetColorAttachment() const = 0; // GLuint texture ID for ImGui::Image
        virtual uint32_t GetDepthAttachment() const = 0; // GLuint texture ID for ImGui::Image

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual uint32_t GetFramebuffer() const = 0;
		virtual uint32_t ReadPixel(uint32_t x, uint32_t y) = 0;
        virtual void ReadPixelRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint32_t>& outIds) = 0;

		virtual void BlitToScreen(int windowWidth, int windowHeight) = 0;
    };

}
