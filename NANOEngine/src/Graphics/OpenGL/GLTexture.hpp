#ifndef NANOENGINE_GRAPHICS_GLTEXTURE_HPP
#define NANOENGINE_GRAPHICS_GLTEXTURE_HPP

#include "NANOEngineAPI.hpp"
#include "../Interfaces/ITexture.hpp"
#include "ResourceManagement/IResource.hpp"
#include <string>
#include <vector>

namespace NE::Graphics::OpenGL {

    NANOENGINE_API std::uint64_t GetClampBindlessHandleForTexture(unsigned int textureId);

	class GLTexture final : public ITexture, public Resource::IResource {
    public:
        GLTexture();
        ~GLTexture();

        //bool LoadFromFile(const std::string& fileName) override;
        bool Preload(NE::Resource::BinaryView blob) override;
        void Finalize() override;

        uint64_t GetBindlessHandle() const override { return m_Handle; }
        uint64_t GetClampBindlessHandle() const { return m_ClampHandle; }
        void MakeResident() override;

        unsigned int GLName() const { return m_ID; }

        static constexpr Resource::ResourceType GetStaticType() { return Resource::ResourceType::Texture; }
        Resource::ResourceType GetType() const override { return GetStaticType(); }

        std::string uuid; // for material serialization
    private:
        unsigned int m_ID = 0;
        uint64_t m_Handle = 0;
        uint64_t m_ClampHandle = 0;

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
