#ifndef NANOENGINE_GRAPHICS_IVERTEX_BUFFER_HPP
#define NANOENGINE_GRAPHICS_IVERTEX_BUFFER_HPP

#include <cstdint>

namespace NANOEngine::Graphics {

    class IVertexBuffer {
    public:
        virtual ~IVertexBuffer() = default;

        virtual void Bind() const = 0;
        virtual void SetData(const void* data, uint32_t size) = 0;
        virtual size_t GetStride() const = 0;
    };

}

#endif // !NANOENGINE_GRAPHICS_IVERTEX_BUFFER_HPP