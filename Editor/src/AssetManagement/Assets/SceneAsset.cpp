#include "pch.h"
#include "SceneAsset.hpp"

#include <Engine.hpp>
#include <ResourceManagement/ResourcePaths.hpp>

#include "../../EditorScene.hpp"
#include "../../Serialization/Serializer.hpp"
#include "../AssetManager.hpp"


namespace Editor::Assets {

    bool SceneAsset::Cook(const std::string& /*sourcePath*/, const std::string& outPath) const {
        NE::CookScene(EditorScene::s_rootOrder, outPath);
        return true;
    }

    bool SceneAsset::LoadImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
        return true;
    }

    bool SceneAsset::SaveImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
        return true;
    }

    bool SceneAsset::SaveScene(const std::string& outPath) {
        Serialization::JSON::SerializeScene(outPath);
        
        auto record = AssetManager::GetInstance().GetRecordBySource(outPath);
        auto path = NE::Resource::ComputeArtifactPathFromUUID(record->id, NE::Resource::ResourceType::Scene);

        return Cook({}, path);
    }

}

