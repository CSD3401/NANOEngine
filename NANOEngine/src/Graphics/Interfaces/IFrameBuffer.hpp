#pragma once
#include <cstdint>

namespace NANOEngine::Graphics {

    class IFrameBuffer {
    public:
        virtual ~IFrameBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void Resize(uint32_t width, uint32_t height) = 0;

        virtual uint32_t GetColorAttachment() const = 0; // GLuint texture ID for ImGui::Image

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
    };

}
