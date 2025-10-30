#include "GLTexture.hpp"
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image/stb_image_resize2.h"
#include "../../Core/Logger.hpp"
#include "ResourceManagement/BinaryHeaders/NanoTexHeader.hpp"

namespace {

    // --- Helpers for BC formats (BC7 here). Adjust if you add others ---
    static inline size_t BCBytesForLevel(uint32_t w, uint32_t h, uint8_t format /*enum*/) {
        // BC7 = 16 bytes per 4x4 block
        const uint32_t blockW = (w + 3) / 4;
        const uint32_t blockH = (h + 3) / 4;
        const size_t bytesPerBlock = 16; // BC7
        (void)format; // map different formats later
        return static_cast<size_t>(blockW) * static_cast<size_t>(blockH) * bytesPerBlock;
    }

    // Computes per-mip offsets/sizes and validates total payload size.
    static bool ComputeMipLayout(uint32_t w, uint32_t h, uint16_t mips,
        uint8_t format, size_t payloadSize,
        std::vector<size_t>& offsets, std::vector<size_t>& sizes) {
        offsets.clear(); sizes.clear();
        offsets.reserve(mips); sizes.reserve(mips);
        size_t off = 0;
        for (uint16_t mip = 0; mip < mips; ++mip) {
            const uint32_t mw = std::max(1u, w >> mip);
            const uint32_t mh = std::max(1u, h >> mip);
            const size_t sz = BCBytesForLevel(mw, mh, format);
            if (off + sz > payloadSize) return false;
            offsets.push_back(off);
            sizes.push_back(sz);
            off += sz;
        }
        return off == payloadSize;
    }

    // Map your runtime header format to GL internal format (BC7 example)
    static GLenum MapFormatToGL(uint8_t fmt, bool srgb) {
        // Assuming fmt encodes BC7; expand as you add more formats.
        // Requires GL_ARB_texture_compression_bptc
        return srgb ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM :
            GL_COMPRESSED_RGBA_BPTC_UNORM;
    }

}

namespace NE::Graphics::OpenGL {
    GLTexture::GLTexture() {}

    GLTexture::~GLTexture() {
        if (m_Handle) glMakeTextureHandleNonResidentARB(m_Handle);
        if (m_ID)     glDeleteTextures(1, &m_ID);
    }

    // Keep parsed info as locals via Preload -> Finalize handoff
    struct ParsedTexture {
        uint32_t w = 0, h = 0;
        uint16_t mips = 1;
        uint8_t  format = 0;
        bool     srgb = false;
        const uint8_t* payload = nullptr;
        size_t payloadSize = 0;
        std::vector<size_t> offsets, sizes;
    };

    // stash between phases
    static thread_local ParsedTexture g_tmpParsed; // simple for now; move to a member if you prefer

    bool GLTexture::Preload(NE::Resource::BinaryView blob) {
        if (blob.size < sizeof(NE::Resource::NanoTexHeader)) return false;
        const auto* hdr = blob.as<NE::Resource::NanoTexHeader>(0);
        if (!hdr) return false;
        if (hdr->magic != NE::Resource::NTEX_MAGIC) return false;                               // 'NTEX'
        if (hdr->importerVersion != NE::Resource::CURRENT_NANOTEX_FORMAT_VERSION) return false;       // version gate

        g_tmpParsed.w = hdr->width;
        g_tmpParsed.h = hdr->height;
        g_tmpParsed.mips = hdr->mipCount ? hdr->mipCount : 1;
        g_tmpParsed.format = hdr->format;
        g_tmpParsed.srgb = (hdr->isSRGB != 0);

        const size_t off = sizeof(NE::Resource::NanoTexHeader);
        const size_t pay = (blob.size > off) ? (blob.size - off) : 0;
        const uint8_t* data = blob.at(off, pay);
        if (!data || pay == 0) return false;

        g_tmpParsed.payload = data;
        g_tmpParsed.payloadSize = pay;

        // compute mip layout to verify payload integrity
        if (!ComputeMipLayout(g_tmpParsed.w, g_tmpParsed.h, g_tmpParsed.mips,
            g_tmpParsed.format, g_tmpParsed.payloadSize,
            g_tmpParsed.offsets, g_tmpParsed.sizes)) {
            LOG_WARNING("GLTexture::Preload: payload size mismatch for NTEX.");
            return false;
        }
        return true;
    }

    void GLTexture::Finalize() {
        // Create storage
        glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
        const GLenum internalFormat = MapFormatToGL(g_tmpParsed.format, g_tmpParsed.srgb);
        glTextureStorage2D(m_ID, g_tmpParsed.mips, internalFormat, g_tmpParsed.w, g_tmpParsed.h);

        // Upload each mip (BC compressed data)
        for (uint16_t mip = 0; mip < g_tmpParsed.mips; ++mip) {
            const uint32_t mw = std::max(1u, g_tmpParsed.w >> mip);
            const uint32_t mh = std::max(1u, g_tmpParsed.h >> mip);
            const size_t   sz = g_tmpParsed.sizes[mip];
            const size_t   off = g_tmpParsed.offsets[mip];
            glCompressedTextureSubImage2D(
                m_ID, mip, 0, 0, mw, mh,
                internalFormat, static_cast<GLsizei>(sz),
                g_tmpParsed.payload + off
            );
        }

        // Sampler state (tweak as needed)
        glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, g_tmpParsed.mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Bindless (optional)
        m_Handle = glGetTextureHandleARB(m_ID);
        glMakeTextureHandleResidentARB(m_Handle);

        // clear TLS staging
        g_tmpParsed = ParsedTexture{};
    }

    //bool GLTexture::LoadFromFile(const std::string& fileName)
    //{
    //    int width, height, channels;
    //    //stbi_set_flip_vertically_on_load(true); to be changed from global state to individually flippable
    //    unsigned char* data = stbi_load(fileName.c_str(), &width, &height, &channels, 4); // force RGBA

    //    if (!data) {
    //        LOG_WARNING("Failed to load texture: " + fileName);
    //        return false;
    //    }

    //    glGenTextures(1, &m_ID);
    //    glBindTexture(GL_TEXTURE_2D, m_ID);

    //    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
    //        GL_RGBA, GL_UNSIGNED_BYTE, data);

    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    //    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //    glGenerateMipmap(GL_TEXTURE_2D);
    //    glBindTexture(GL_TEXTURE_2D, 0);

    //    stbi_image_free(data);

    //    // Bindless handle
    //    m_Handle = glGetTextureHandleARB(m_ID);
    //    glMakeTextureHandleResidentARB(m_Handle);

    //    return true;
    //}

    void GLTexture::MakeResident() {
        glMakeTextureHandleResidentARB(m_Handle);
    }
}