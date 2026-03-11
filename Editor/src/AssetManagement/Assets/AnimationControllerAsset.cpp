#include "pch.h"
#include "AnimationControllerAsset.hpp"


namespace Editor::Assets {

    bool AnimationControllerAsset::Cook(const std::string& /*sourcePath*/, const std::string& /*outPath*/) const {
        return true;
    }

    bool AnimationControllerAsset::LoadImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
        return true;
    }

    bool AnimationControllerAsset::SaveImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
        return true;
    }
}

