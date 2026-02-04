#include "GLUtilities.hpp"

#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image/stb_image_resize2.h"

namespace Engine {
    GLuint CreateGLTexture(const std::string& path, int targetSize) {
        int srcWidth, srcHeight, srcChannels;
        stbi_uc* originalData = stbi_load(path.c_str(), &srcWidth, &srcHeight, &srcChannels, 4); // Force RGBA
        if (!originalData)
            return 0;

        // --- Step 1: Calculate aspect ratio preserved size ---
        int resizedWidth = targetSize;
        int resizedHeight = targetSize;

        if (srcWidth > srcHeight) {
            resizedHeight = static_cast<int>((float)srcHeight / srcWidth * targetSize);
        } else if (srcHeight > srcWidth) {
            resizedWidth = static_cast<int>((float)srcWidth / srcHeight * targetSize);
        }

        // --- Step 2: Resize original image to resizedWidth x resizedHeight ---
        stbi_uc* resizedData = new stbi_uc[resizedWidth * resizedHeight * 4];
        stbir_resize_uint8_linear(
            originalData,
            srcWidth, srcHeight, 0,  // 0 = tightly packed
            resizedData,
            resizedWidth, resizedHeight, 0,
            STBIR_RGBA  // 4 channels = RGBA
        );


        // --- Step 3: Center resized image into targetSize x targetSize canvas ---
        stbi_uc* finalData = new stbi_uc[targetSize * targetSize * 4];
        memset(finalData, 0, targetSize * targetSize * 4); // Transparent black

        int xOffset = (targetSize - resizedWidth) / 2;
        int yOffset = (targetSize - resizedHeight) / 2;

        for (int y = 0; y < resizedHeight; y++) {
            memcpy(
                finalData + ((y + yOffset) * targetSize + xOffset) * 4,
                resizedData + (y * resizedWidth) * 4,
                resizedWidth * 4
            );
        }

        // --- Step 4: Upload to OpenGL ---
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, targetSize, targetSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, finalData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_2D, 0);

        // --- Step 5: Cleanup ---
        stbi_image_free(originalData);
        delete[] resizedData;
        delete[] finalData;

        return textureID;
    }
}