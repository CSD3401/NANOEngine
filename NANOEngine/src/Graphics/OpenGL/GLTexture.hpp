#ifndef NANOENGINE_GRAPHICS_GLTEXTURE_HPP
#define NANOENGINE_GRAPHICS_GLTEXTURE_HPP

#include "../Interfaces/ITexture.hpp"
#include <string>
#include <vector>
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

        struct ParsedTexture {
            uint32_t w = 0, h = 0;
            uint16_t mips = 1;
            uint8_t  format = 0;
            bool     srgb = false;
            const uint8_t* payload = nullptr;
            size_t payloadSize = 0;
            std::vector<size_t> offsets, sizes;
        };

        ParsedTexture m_stage;
	};

}

#endif // !NANOENGINE_GRAPHICS_GLTEXTURE_HPP