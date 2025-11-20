#ifndef NANOENGINE_GRAPHICS_IGEOMETRY_BUFFER_HPP
#define NANOENGINE_GRAPHICS_IGEOMETRY_BUFFER_HPP

namespace NE::Graphics {

    class IGeometryBuffer {
    public:
        virtual ~IGeometryBuffer() = default;

        virtual void Bind() const = 0;
        virtual void Draw() const = 0;
        virtual void Unbind() const = 0;
        virtual void EnableInstanceLayout(int locModel, int locIdRGB) = 0;
        virtual void DrawInstanced(size_t instanceCount) const = 0;
    };


}

#endif // !NANOENGINE_GRAPHICS_IGEOMETRY_BUFFER_HPP