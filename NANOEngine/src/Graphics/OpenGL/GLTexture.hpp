#ifndef NANOENGINE_GRAPHICS_GLTEXTURE_HPP
#define NANOENGINE_GRAPHICS_GLTEXTURE_HPP

#include "../Interfaces/ITexture.hpp"
#include <string>

namespace NE::Graphics::OpenGL {
	class GLTexture : public ITexture {
        using GLuint = unsigned int;
        using GLuint64 = uint64_t;
    public:
        GLTexture();
        ~GLTexture();

        bool LoadFromFile(const std::string& fileName) override;

        uint64_t GetBindlessHandle() const override { return m_Handle; }
        void MakeResident() override;

        GLuint GLName() const { return m_ID; }

    private:
        GLuint m_ID = 0;
        GLuint64 m_Handle = 0;
	};
}

#endif // !NANOENGINE_GRAPHICS_GLTEXTURE_HPP