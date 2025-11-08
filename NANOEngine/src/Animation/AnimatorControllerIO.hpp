// AnimatorControllerIO.hpp
#pragma once
#include "../NANOEngineAPI.hpp"          // <-- add this include
#include "AnimatorController.hpp"
#include <string>

namespace NE::Animation {
    NANOENGINE_API bool SaveAnimatorController(const AnimatorController& c, const std::string& path);
    NANOENGINE_API bool LoadAnimatorController(AnimatorController& c, const std::string& path);
}
