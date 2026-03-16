#include "pch.h"
#include "GLTexture.hpp"
#include <glad/glad.h>
#include "Core/SpdLogger.hpp"
#include "ResourceManagement/BinaryHeaders/NanoTexHeader.hpp"

namespace {
    enum class NanoTexFormat : uint8_t {
        BC7_UNORM = 0,
        BC7_UNORM_SRGB = 1,
        BC5_UNORM = 2,
        BC6H_UF16 = 3
    };

    GLuint EnsureClampSampler() {
        static GLuint s_ClampSampler = 0;
        if (s_ClampSampler != 0) {
            return s_ClampSampler;
        }

        glCreateSamplers(1, &s_ClampSampler);
        glSamplerParameteri(s_ClampSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glSamplerParameteri(s_ClampSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glSamplerParameteri(s_ClampSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glSamplerParameteri(s_ClampSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return s_ClampSampler;
    }

    // BC5, BC6H, and BC7 all use 16-byte 4x4 blocks.
    static inline size_t BCBytesForLevel(uint32_t w, uint32_t h, uint8_t format /*enum*/) {
        const uint32_t blockW = (w + 3) / 4;
        const uint32_t blockH = (h + 3) / 4;
        const size_t bytesPerBlock = 16;
        (void)format;
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

    static GLenum MapFormatToGL(uint8_t fmt, bool srgb)
    {
        switch (static_cast<NanoTexFormat>(fmt)) {
        case NanoTexFormat::BC7_UNORM:
            return srgb ? GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
                : GL_COMPRESSED_RGBA_BPTC_UNORM;

        case NanoTexFormat::BC7_UNORM_SRGB:
            return GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;

        case NanoTexFormat::BC5_UNORM:
            return GL_COMPRESSED_RG_RGTC2;

        case NanoTexFormat::BC6H_UF16:
            if (srgb) {
                SPD_WARNING("NanoTex BC6H payload requested sRGB decode. Ignoring sRGB flag for HDR texture.");
            }
            return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;

        default:
            SPD_WARNING("Unknown NanoTex format {}, defaulting to BC7 UNORM", (int)fmt);
            return GL_COMPRESSED_RGBA_BPTC_UNORM;
        }
    }

}

namespace NE::Graphics::OpenGL {
    std::uint64_t GetClampBindlessHandleForTexture(unsigned int textureId) {
        if (textureId == 0u) {
            return 0;
        }

        const std::uint64_t handle = glGetTextureSamplerHandleARB(textureId, EnsureClampSampler());
        if (handle != 0u) {
            glMakeTextureHandleResidentARB(handle);
        }
        return handle;
    }

    GLTexture::GLTexture() {}

    GLTexture::~GLTexture() {
        if (m_Handle) glMakeTextureHandleNonResidentARB(m_Handle);
        if (m_ClampHandle) glMakeTextureHandleNonResidentARB(m_ClampHandle);
        if (m_ID)     glDeleteTextures(1, &m_ID);
    }

    bool GLTexture::Preload(NE::Resource::BinaryView blob) {
        if (blob.size < sizeof(NE::Resource::NanoTexHeader)) return false;
        const auto* hdr = blob.as<NE::Resource::NanoTexHeader>(0);
        if (!hdr) return false;
        if (hdr->magic != NE::Resource::NTEX_MAGIC) return false;                               // 'NTEX'
        if (hdr->importerVersion != NE::Resource::CURRENT_NANOTEX_FORMAT_VERSION) return false;       // version gate

        m_stage.w = hdr->width;
        m_stage.h = hdr->height;
        m_stage.mips = hdr->mipCount ? hdr->mipCount : 1;
        m_stage.format = hdr->format;
        m_stage.srgb = (hdr->isSRGB != 0);
        if (static_cast<NanoTexFormat>(m_stage.format) == NanoTexFormat::BC6H_UF16) {
            m_stage.srgb = false;
        }

        const size_t off = sizeof(NE::Resource::NanoTexHeader);
        const size_t pay = (blob.size > off) ? (blob.size - off) : 0;
        const uint8_t* data = blob.at(off, pay);
        if (!data || pay == 0) return false;

        m_stage.payload = data;
        m_stage.payloadSize = pay;

        // compute mip layout to verify payload integrity
        if (!ComputeMipLayout(m_stage.w, m_stage.h, m_stage.mips,
            m_stage.format, m_stage.payloadSize,
            m_stage.offsets, m_stage.sizes)) {
            SPD_WARNING("GLTexture::Preload: payload size mismatch for NTEX.");
            return false;
        }
        return true;
    }

    void GLTexture::Finalize() {
        // Create storage
        glCreateTextures(GL_TEXTURE_2D, 1, &m_ID);
        const GLenum internalFormat = MapFormatToGL(m_stage.format, m_stage.srgb);
        glTextureStorage2D(m_ID, m_stage.mips, internalFormat, m_stage.w, m_stage.h);

        // Upload each mip (BC compressed data)
        for (uint16_t mip = 0; mip < m_stage.mips; ++mip) {
            const uint32_t mw = std::max(1u, m_stage.w >> mip);
            const uint32_t mh = std::max(1u, m_stage.h >> mip);
            const size_t   sz = m_stage.sizes[mip];
            const size_t   off = m_stage.offsets[mip];
            glCompressedTextureSubImage2D(
                m_ID, mip, 0, 0, mw, mh,
                internalFormat, static_cast<GLsizei>(sz),
                m_stage.payload + off
            );
        }

        // Sampler state (tweak as needed)
        glTextureParameteri(m_ID, GL_TEXTURE_MIN_FILTER, m_stage.mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTextureParameteri(m_ID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_ID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(m_ID, GL_TEXTURE_WRAP_T, GL_REPEAT);

        // Bindless (optional)
        m_Handle = glGetTextureHandleARB(m_ID);
        glMakeTextureHandleResidentARB(m_Handle);
        m_ClampHandle = glGetTextureSamplerHandleARB(m_ID, EnsureClampSampler());
        glMakeTextureHandleResidentARB(m_ClampHandle);

        // clear TLS staging
        m_stage = ParsedTexture{};
    }

    void GLTexture::MakeResident() {
        glMakeTextureHandleResidentARB(m_Handle);
        if (m_ClampHandle != 0) {
            glMakeTextureHandleResidentARB(m_ClampHandle);
        }
    }
}
