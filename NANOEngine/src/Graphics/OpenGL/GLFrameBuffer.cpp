#include "GLFrameBuffer.hpp"
#include <glad/glad.h>
#include "../../src/Core/Logger.hpp"
#include "ECS/Core/Entity.hpp"

namespace NE::Graphics::OpenGL {
    GLFrameBuffer::GLFrameBuffer()
    {
    }

    GLFrameBuffer::GLFrameBuffer(uint32_t width, uint32_t height)
        : m_Width(width), m_Height(height)
    {
        Invalidate();
    }

    GLFrameBuffer::~GLFrameBuffer()
    {
        DestroyAttachments();
    }

    void GLFrameBuffer::DestroyAttachments()
    {
        if (m_FBO) {
            glDeleteFramebuffers(1, &m_FBO);
            m_FBO = 0;
        }
        if (m_ColorAttachment) {
            glDeleteTextures(1, &m_ColorAttachment);
            m_ColorAttachment = 0;
        }
        if (m_PickingAttachment) {
            glDeleteTextures(1, &m_PickingAttachment);
            m_PickingAttachment = 0;
        }
        if (m_NormalAttachment) {
            glDeleteTextures(1, &m_NormalAttachment);
            m_NormalAttachment = 0;
        }
        if (m_DepthAttachment) {
            glDeleteTextures(1, &m_DepthAttachment);
            m_DepthAttachment = 0;
        }
    }

    void GLFrameBuffer::Configure(FormatMode mode, uint32_t width, uint32_t height, bool enablePicking, bool enableMiniGBuffer, bool enableDepth, bool enableStencil)
    {
        m_Mode = mode;
        m_Width = width;
        m_Height = height;
        m_EnablePicking = enablePicking;
        m_EnableMiniGBuffer = enableMiniGBuffer;
        m_EnableDepth = enableDepth;
        m_EnableStencil = enableStencil;
        m_PickingWriteEnabled = enablePicking;
        RebuildAttachments();
    }

    void GLFrameBuffer::CreateAsStandard(uint32_t width, uint32_t height, bool enablePicking, bool enableMiniGBuffer, bool enableDepth, bool enableStencil)
    {
        Configure(FormatMode::Standard, width, height, enablePicking, enableMiniGBuffer, enableDepth, enableStencil);
    }

    void GLFrameBuffer::CreateAsHDR(uint32_t width, uint32_t height, bool enablePicking, bool enableMiniGBuffer, bool enableDepth, bool enableStencil)
    {
        Configure(FormatMode::HDR, width, height, enablePicking, enableMiniGBuffer, enableDepth, enableStencil);
    }

    void GLFrameBuffer::CreateAsLDR(uint32_t width, uint32_t height, bool enablePicking, bool enableMiniGBuffer, bool enableDepth, bool enableStencil)
    {
        Configure(FormatMode::LDR, width, height, enablePicking, enableMiniGBuffer, enableDepth, enableStencil);
    }

    void GLFrameBuffer::RebuildAttachments()
    {
        DestroyAttachments();

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        const GLint colorInternalFormat = (m_Mode == FormatMode::HDR) ? GL_RGBA16F : GL_RGBA8;
        const GLenum colorDataType = (m_Mode == FormatMode::HDR) ? GL_FLOAT : GL_UNSIGNED_BYTE;

        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, colorInternalFormat, m_Width, m_Height, 0, GL_RGBA, colorDataType, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        if (m_EnablePicking) {
            glGenTextures(1, &m_PickingAttachment);
            glBindTexture(GL_TEXTURE_2D, m_PickingAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_PickingAttachment, 0);
        }

        if (m_EnableMiniGBuffer) {
            glGenTextures(1, &m_NormalAttachment);
            glBindTexture(GL_TEXTURE_2D, m_NormalAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, m_NormalAttachment, 0);
        }

        if (m_EnableDepth) {
            glGenTextures(1, &m_DepthAttachment);
            glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
            if (m_EnableStencil) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_Width, m_Height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
            }
            else {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_Width, m_Height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        ApplyDrawBuffers();

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_ERROR("Framebuffer is incomplete!");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFrameBuffer::Invalidate()
    {
        RebuildAttachments();
    }

    void GLFrameBuffer::ApplyDrawBuffers() const
    {
        if (m_EnablePicking && m_PickingWriteEnabled && m_PickingAttachment != 0) {
            const GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
            glDrawBuffers(2, attachments);
            return;
        }

        const GLenum attachment = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &attachment);
    }

    void GLFrameBuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glViewport(0, 0, m_Width, m_Height);
    }

    void GLFrameBuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
            return;

        m_Width = width;
        m_Height = height;
        RebuildAttachments();
    }

    void GLFrameBuffer::Clear()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

        GLbitfield clearMask = GL_COLOR_BUFFER_BIT;
        if (m_EnableDepth) {
            clearMask |= GL_DEPTH_BUFFER_BIT;
        }
        if (m_EnableDepth && m_EnableStencil) {
            clearMask |= GL_STENCIL_BUFFER_BIT;
        }
        glClear(clearMask);
    }

    void GLFrameBuffer::SetPickingWrite(bool enable)
    {
        m_PickingWriteEnabled = enable;
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        ApplyDrawBuffers();
    }

    uint32_t GLFrameBuffer::ReadPixel(uint32_t x, uint32_t y)
    {
        if (!m_PickingAttachment) return ECS::NO_ENTITY;

        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glReadBuffer(GL_COLOR_ATTACHMENT1);

        uint8_t data[4] = { 0, 0, 0, 0 };
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        uint32_t id = data[0] | (data[1] << 8) | (data[2] << 16);
        if (id > ECS::MAX_ENTITIES) return ECS::NO_ENTITY;

        return id;
    }

    void GLFrameBuffer::ReadPixelRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, std::vector<uint32_t>& outIds)
    {
        if (!m_PickingAttachment || width == 0 || height == 0) {
            outIds.clear();
            return;
        }

        const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
        const size_t byteCount = pixelCount * 4;

        std::vector<uint8_t> raw;
        raw.resize(byteCount);

        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        glReadPixels(
            x, y,
            width, height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            raw.data()
        );

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        for (size_t i = 0; i < pixelCount; ++i) {
            uint8_t r = raw[i * 4 + 0];
            uint8_t g = raw[i * 4 + 1];
            uint8_t b = raw[i * 4 + 2];

            uint32_t id = r | (g << 8) | (b << 16);

            if (id > ECS::MAX_ENTITIES || id == ECS::NO_ENTITY)
                continue;

            bool found = false;
            for (uint32_t existingId : outIds) {
                if (existingId == id) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                outIds.push_back(id);
            }
        }
    }

    void GLFrameBuffer::BlitToScreen(int windowWidth, int windowHeight)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, windowWidth, windowHeight);
        glBlitFramebuffer(
            0, 0, m_Width, m_Height,
            0, 0, windowWidth, windowHeight,
            GL_COLOR_BUFFER_BIT, GL_LINEAR
        );
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFrameBuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
