// NANOEngine/src/Animation/TransformClipIO.hpp
#pragma once
#include "../NANOEngineAPI.hpp"  
#include "TransformClip.hpp"
#include <string>

namespace NE::Animation {
    NANOENGINE_API bool SaveTransformClip(const TransformClip& c, const std::string& path);
    NANOENGINE_API bool LoadTransformClip(TransformClip& c, const std::string& path);
}
