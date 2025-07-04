#ifndef NANOENGINE_GRAPHICS_IGEOMETRY_BUFFER_HPP
#define NANOENGINE_GRAPHICS_IGEOMETRY_BUFFER_HPP

namespace NANOEngine::Graphics {

    class IGeometryBuffer {
    public:
        virtual ~IGeometryBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Draw() const = 0;
    };


}

#endif // !NANOENGINE_GRAPHICS_IGEOMETRY_BUFFER_HPP