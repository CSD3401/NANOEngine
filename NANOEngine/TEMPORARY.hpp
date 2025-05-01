#include <string>
#include "NANOEngineAPI.hpp"

typedef unsigned int GLuint;

namespace NANOEngine {
    NANOENGINE_API GLuint LoadTextureFromFile(const std::string& path);
}
