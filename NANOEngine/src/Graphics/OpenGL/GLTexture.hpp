#ifndef NANOENGINE_GRAPHICS_GLTEXTURE_HPP
#define NANOENGINE_GRAPHICS_GLTEXTURE_HPP

#include "../Interfaces/ITexture.hpp"
#include <string>
#include "ResourceManagement/BinaryView.hpp"

namespace NE::Graphics::OpenGL {

	class GLTexture final : public ITexture {
    public:
        GLTexture();
        ~GLTexture();

        //bool LoadFromFile(const std::string& fileName) override;
        bool Preload(NE::Resource::BinaryView blob) override;
        void Finalize() override;

        uint64_t GetBindlessHandle() const override { return m_Handle; }
        void MakeResident() override;

        unsigned int GLName() const { return m_ID; }

    private:
        unsigned int m_ID = 0;
        uint64_t m_Handle = 0;
	};

}

#endif // !NANOENGINE_GRAPHICS_GLTEXTURE_HPP