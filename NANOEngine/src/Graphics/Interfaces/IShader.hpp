#ifndef NANOENGINE_GRAPHICS_ISHADER_HPP
#define NANOENGINE_GRAPHICS_ISHADER_HPP

#include <string>

namespace NE::Math {
    struct Vec3;
    struct Mat4;
}

namespace NE::Graphics {
    using NE::Math::Vec3;
    using NE::Math::Mat4;

    class IShader {
    public:
        virtual ~IShader() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetUniformInt(const std::string& name, int value) = 0;
        virtual void SetUniformFloat(const std::string& name, float value) = 0;
        virtual void SetUniformVec3(const std::string& name, const Vec3& value) = 0;
        virtual void SetUniformMat4(const std::string& name, const Mat4& matrix) = 0;

        virtual void SetUniformHandle(const std::string& uName, uint64_t handle) = 0;
        virtual void SetUniformHandlev(const std::string& uName, const uint64_t* handles, int count) = 0;

		virtual const std::string_view GetUUID() const = 0;
    };

}

#endif // !NANOENGINE_GRAPHICS_ISHADER_HPP