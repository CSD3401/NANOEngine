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
        glDeleteFramebuffers(1, &m_FBO);
        glDeleteTextures(1, &m_ColorAttachment);
		glDeleteTextures(1, &m_PickingAttachment);
        glDeleteRenderbuffers(1, &m_RBO);
    }

    void GLFrameBuffer::CreateAsHDR(uint32_t width, uint32_t height, bool enablePicking) {
        m_Width = width;
        m_Height = height;

        if (m_FBO) {
            glDeleteFramebuffers(1, &m_FBO);
            glDeleteTextures(1, &m_ColorAttachment);
            glDeleteRenderbuffers(1, &m_RBO);
        }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // --- Color attachment 0: normal scene ---
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        // --- Color attachment 1: picking IDs ---
        if (enablePicking) {
            glGenTextures(1, &m_PickingAttachment);
            glBindTexture(GL_TEXTURE_2D, m_PickingAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_PickingAttachment, 0);
        }

        // Specify the color attachments for rendering
        GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, attachments);

        // --- Depth-stencil attachment ---
        glGenRenderbuffers(1, &m_RBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            LOG_ERROR("Framebuffer is incomplete!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFrameBuffer::CreateAsLDR(uint32_t width, uint32_t height, bool enablePicking) {
        if (m_FBO) {
            glDeleteFramebuffers(1, &m_FBO);
            glDeleteTextures(1, &m_ColorAttachment);
            glDeleteRenderbuffers(1, &m_RBO);
        }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // --- Color attachment 0: normal scene ---
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        // --- Color attachment 1: picking IDs ---
        if (enablePicking) {
            glGenTextures(1, &m_PickingAttachment);
            glBindTexture(GL_TEXTURE_2D, m_PickingAttachment);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_PickingAttachment, 0);
        }

        // Specify the color attachments for rendering
        GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, attachments);

        // --- Depth-stencil attachment ---
        glGenRenderbuffers(1, &m_RBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            LOG_ERROR("Framebuffer is incomplete!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void GLFrameBuffer::Invalidate() 
    {
        if (m_FBO) {
            glDeleteFramebuffers(1, &m_FBO);
            glDeleteTextures(1, &m_ColorAttachment);
            glDeleteRenderbuffers(1, &m_RBO);
        }

        glGenFramebuffers(1, &m_FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

        // --- Color attachment 0: normal scene ---
        glGenTextures(1, &m_ColorAttachment);
        glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

        // --- Color attachment 1: picking IDs ---
        glGenTextures(1, &m_PickingAttachment);
        glBindTexture(GL_TEXTURE_2D, m_PickingAttachment);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_Width, m_Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_PickingAttachment, 0);

		// Specify the color attachments for rendering
		GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
		glDrawBuffers(2, attachments);

		// --- Depth-stencil attachment ---
        glGenRenderbuffers(1, &m_RBO);
        glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Width, m_Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            LOG_ERROR("Framebuffer is incomplete!");

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
        Invalidate();
    }

    void GLFrameBuffer::Clear() 
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}

    void GLFrameBuffer::SetPickingWrite(bool enable) 
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        if (enable) {
            GLenum attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
            glDrawBuffers(2, attachments);
        }
        else {
            GLenum attachments[1] = { GL_COLOR_ATTACHMENT0 };
            glDrawBuffers(1, attachments);
        }
	}

    uint32_t GLFrameBuffer::ReadPixel(uint32_t x, uint32_t y) 
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
        glReadBuffer(GL_COLOR_ATTACHMENT1); // Picking attachment
        uint8_t data[4] = { 0, 0, 0, 0 };
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        uint32_t id = data[0] | (data[1] << 8) | (data[2] << 16);

        if (id > ECS::MAX_ENTITIES) return ECS::NO_ENTITY;

        return id;
	}

    void GLFrameBuffer::Unbind()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

}
