#include "PrefabAsset.hpp"

#include <Engine.hpp>
#include <ResourceManagement/ResourcePaths.hpp>


#include "../../EditorScene.hpp"
#include "../../Serialization/Serializer.hpp"
#include "../AssetManager.hpp"


namespace Editor::Assets {

	bool PrefabAsset::Cook(const std::string& sourcePath,
		const std::string& outPath) const {
		NE::CookPrefab(EditorScene::s_selection.GetLastPreorder().front(), outPath);
		return true;
	}

	bool PrefabAsset::LoadImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
		return true;
	}

	bool PrefabAsset::SaveImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
		return true;
	}

	bool PrefabAsset::SavePrefab(const std::string& outPath) {
		Serialization::JSON::SerializePrefab(outPath);

		auto record = AssetManager::GetInstance().GetRecordBySource(outPath);
		auto path = NE::Resource::ComputeArtifactPathFromUUID(record->id, NE::Resource::ResourceType::Prefab);

		return Cook({}, path);
	}

}
