#ifndef NANOENGINE_GRAPHICS_ISHADER_HPP
#define NANOENGINE_GRAPHICS_ISHADER_HPP

#include <string>
#include "../../Asset.hpp"

namespace NANOEngine::Math {
    struct Vec3;
    struct Mat4;
}

namespace NANOEngine::Graphics {
    using NANOEngine::Math::Vec3;
    using NANOEngine::Math::Mat4;

    class IShader : public virtual Asset::IAsset {
    public:
        virtual ~IShader() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void SetUniformInt(const std::string& name, int value) = 0;
        virtual void SetUniformFloat(const std::string& name, float value) = 0;
        virtual void SetUniformVec3(const std::string& name, const Vec3& value) = 0;
        virtual void SetUniformMat4(const std::string& name, const Mat4& matrix) = 0;

		virtual const std::string_view GetUUID() const = 0;
    };

}

#endif // !NANOENGINE_GRAPHICS_ISHADER_HPP