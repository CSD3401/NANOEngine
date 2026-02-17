#include "TexturePool.hpp"

#include <glad/glad.h>

#include "Core/SpdLogger.hpp"

namespace NE::Graphics {
    namespace {
        GLenum GetGLInternalFormat(TextureFormat format) {
            switch (format) {
            case TextureFormat::R8:             return GL_R8;
            case TextureFormat::RG8:            return GL_RG8;
            case TextureFormat::RGB8:           return GL_RGB8;
            case TextureFormat::RGBA8:          return GL_RGBA8;
            case TextureFormat::R16F:           return GL_R16F;
            case TextureFormat::RG16F:          return GL_RG16F;
            case TextureFormat::RGB16F:         return GL_RGB16F;
            case TextureFormat::RGBA16F:        return GL_RGBA16F;
            case TextureFormat::R32F:           return GL_R32F;
            case TextureFormat::RG32F:          return GL_RG32F;
            case TextureFormat::RGB32F:         return GL_RGB32F;
            case TextureFormat::RGBA32F:        return GL_RGBA32F;
            case TextureFormat::Depth24:        return GL_DEPTH_COMPONENT24;
            case TextureFormat::Depth32F:       return GL_DEPTH_COMPONENT32F;
            case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
            default:                            return GL_RGBA8;
            }
        }

        GLenum GetGLFormat(TextureFormat format) {
            switch (format) {
            case TextureFormat::R8:
            case TextureFormat::R16F:
            case TextureFormat::R32F:
                return GL_RED;
            case TextureFormat::RG8:
            case TextureFormat::RG16F:
            case TextureFormat::RG32F:
                return GL_RG;
            case TextureFormat::RGB8:
            case TextureFormat::RGB16F:
            case TextureFormat::RGB32F:
                return GL_RGB;
            case TextureFormat::RGBA8:
            case TextureFormat::RGBA16F:
            case TextureFormat::RGBA32F:
                return GL_RGBA;
            case TextureFormat::Depth24:
            case TextureFormat::Depth32F:
                return GL_DEPTH_COMPONENT;
            case TextureFormat::Depth24Stencil8:
                return GL_DEPTH_STENCIL;
            default:
                return GL_RGBA;
            }
        }

        GLenum GetGLType(TextureFormat format) {
            switch (format) {
            case TextureFormat::R8:
            case TextureFormat::RG8:
            case TextureFormat::RGB8:
            case TextureFormat::RGBA8:
                return GL_UNSIGNED_BYTE;
            case TextureFormat::R16F:
            case TextureFormat::RG16F:
            case TextureFormat::RGB16F:
            case TextureFormat::RGBA16F:
            case TextureFormat::R32F:
            case TextureFormat::RG32F:
            case TextureFormat::RGB32F:
            case TextureFormat::RGBA32F:
            case TextureFormat::Depth32F:
                return GL_FLOAT;
            case TextureFormat::Depth24:
                return GL_UNSIGNED_INT;
            case TextureFormat::Depth24Stencil8:
                return GL_UNSIGNED_INT_24_8;
            default:
                return GL_UNSIGNED_BYTE;
            }
        }

        bool IsDepthFormat(TextureFormat format) {
            return format == TextureFormat::Depth24 ||
                format == TextureFormat::Depth32F ||
                format == TextureFormat::Depth24Stencil8;
        }
    }

    TexturePool::~TexturePool() {
        Clear();
    }

    PooledTexture* TexturePool::Acquire(uint32_t width, uint32_t height, TextureFormat format) {
        for (auto& tex : m_Textures) {
            if (!tex->inUse &&
                tex->width == width &&
                tex->height == height &&
                tex->format == format) {
                tex->inUse = true;
#ifndef PRODUCTION_BUILD
                m_Stats.hits++;
                m_Stats.inUseCount++;
#endif
                return tex.get();
            }
        }

#ifndef PRODUCTION_BUILD
        m_Stats.misses++;
        m_Stats.totalAllocated++;
#endif

        auto pooledTex = std::make_unique<PooledTexture>();
        pooledTex->width = width;
        pooledTex->height = height;
        pooledTex->format = format;
        pooledTex->inUse = true;

        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);

        GLenum internalFormat = GetGLInternalFormat(format);
        GLenum glFormat = GetGLFormat(format);
        GLenum type = GetGLType(format);

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
            width, height, 0,
            glFormat, type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);

        pooledTex->textureId = textureId;

        GLuint fboId;
        glGenFramebuffers(1, &fboId);
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);

        if (IsDepthFormat(format)) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D, textureId, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        } else {
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_2D, textureId, 0);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            SPD_ERROR("TexturePool: Failed to create FBO, status: {}", status);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        pooledTex->fboId = fboId;

        PooledTexture* result = pooledTex.get();
        m_Textures.push_back(std::move(pooledTex));
#ifndef PRODUCTION_BUILD
        m_Stats.poolSize++;
        m_Stats.inUseCount++;
#endif

        return result;
    }

    void TexturePool::Release(PooledTexture* texture) {
        if (texture && texture->inUse) {
            texture->inUse = false;
#ifndef PRODUCTION_BUILD
            if (m_Stats.inUseCount > 0) {
                m_Stats.inUseCount--;
            }
#endif
        }
    }

    void TexturePool::ReleaseAll() {
        for (auto& tex : m_Textures) {
            tex->inUse = false;
        }
#ifndef PRODUCTION_BUILD
        m_Stats.inUseCount = 0;
#endif
    }

    void TexturePool::Clear() {
        for (auto& tex : m_Textures) {
            if (tex->fboId != 0) {
                glDeleteFramebuffers(1, &tex->fboId);
            }
            if (tex->textureId != 0) {
                glDeleteTextures(1, &tex->textureId);
            }
        }
        m_Textures.clear();
#ifndef PRODUCTION_BUILD
        m_Stats = TexturePoolStats{};
#endif
    }
}