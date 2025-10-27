#include "GLTexture.hpp"
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image/stb_image_resize2.h"
#include "../../Core/Logger.hpp"

namespace NE::Graphics::OpenGL {
    GLTexture::GLTexture() {

    }

    GLTexture::~GLTexture() {
        if (m_Handle) glMakeTextureHandleNonResidentARB(m_Handle);
        if (m_ID)     glDeleteTextures(1, &m_ID);
    }

    bool GLTexture::LoadFromFile(const std::string& fileName)
    {
        int width, height, channels;
        //stbi_set_flip_vertically_on_load(true); to be changed from global state to individually flippable
        unsigned char* data = stbi_load(fileName.c_str(), &width, &height, &channels, 4); // force RGBA

        if (!data) {
            LOG_WARNING("Failed to load texture: " + fileName);
            return false;
        }

        glGenTextures(1, &m_ID);
        glBindTexture(GL_TEXTURE_2D, m_ID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(data);

        // Bindless handle
        m_Handle = glGetTextureHandleARB(m_ID);
        glMakeTextureHandleResidentARB(m_Handle);

        return true;
    }

    void GLTexture::MakeResident() {
        glMakeTextureHandleResidentARB(m_Handle);
    }
}