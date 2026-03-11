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
        virtual uint32_t GetPickingAttachment() const = 0; // GLuint texture ID for ImGui::Image (0 if unavailable)
        virtual uint32_t GetDepthAttachment() const = 0; // GLuint texture ID for ImGui::Image
        virtual uint32_t GetNormalAttachment() const = 0; // optional mini-gbuffer normal texture
        virtual uint32_t GetRoughnessAttachment() const = 0; // optional mini-gbuffer roughness texture
        virtual bool HasMiniGBuffer() const = 0;
        virtual bool HasDepth() const = 0;
        virtual bool HasStencil() const = 0;
        virtual bool HasPickingAttachment() const = 0;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;

        virtual uint32_t GetFramebuffer() const = 0;
		virtual uint32_t ReadPixel(uint32_t x, uint32_t y) = 0;
        virtual void ReadPixelRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint32_t>& outIds) = 0;

		virtual void BlitToScreen(int windowWidth, int windowHeight) = 0;
    };

}
