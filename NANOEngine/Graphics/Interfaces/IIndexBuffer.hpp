#ifndef NANOENGINE_GRAPHICS_IINDEX_BUFFER_HPP
#define NANOENGINE_GRAPHICS_IINDEX_BUFFER_HPP

#include <cstdint>

namespace NANOEngine::Graphics {

    class IIndexBuffer {
    public:
        virtual ~IIndexBuffer() = default;

        virtual void Bind() const = 0;
        virtual size_t GetCount() const = 0;
    };


}

#endif // !NANOENGINE_GRAPHICS_IINDEX_BUFFER_HPP