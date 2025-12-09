#include "SceneAsset.hpp"

namespace Editor::Assets {

    bool SceneAsset::Cook(const std::string& sourcePath, const std::string& outPath) const {
        return false;
    }

    bool SceneAsset::LoadImportSettings(const std::string& sourcePath) { // No Import Settings
        return true;
    }

    bool SceneAsset::SaveImportSettings(const std::string& sourcePath) { // No Import Settings
        return true;
    }

}

