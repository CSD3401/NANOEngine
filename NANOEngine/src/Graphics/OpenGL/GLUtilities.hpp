#pragma once
#include <string>
#include "../../NANOEngineAPI.hpp"

typedef unsigned int GLuint;

namespace Engine {
    NANOENGINE_API GLuint CreateGLTexture(const std::string& path, int targetSize = 128);
}
