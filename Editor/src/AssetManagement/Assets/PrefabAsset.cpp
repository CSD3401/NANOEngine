#include "PrefabAsset.hpp"

#include <Engine.hpp>
#include <ResourceManagement/ResourcePaths.hpp>


#include "../../EditorScene.hpp"
#include "../../Serialization/Serializer.hpp"
#include "../AssetManager.hpp"


namespace Editor::Assets {

	bool PrefabAsset::Cook(const std::string& sourcePath,
		const std::string& outPath) const {

		if (EditorScene::s_selection.GetLastDropped() == NE::ECS::NO_ENTITY && !m_isScene) {
			SPD_ERROR("No entity selected to cook prefab from: {}", sourcePath);
			return false;
		}

		if (m_isScene)
			NE::CookPrefab(EditorScene::s_rootOrder[0], outPath);
		else
			NE::CookPrefab(EditorScene::s_selection.GetLastDropped(), outPath);
		return true;
	}

	bool PrefabAsset::LoadImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
		return true;
	}

	bool PrefabAsset::SaveImportSettings(const std::string& /*sourcePath*/) { // No Import Settings
		return true;
	}

	bool PrefabAsset::SavePrefab(const std::string& outPath, bool isScene) {
		m_isScene = isScene;
		auto record = AssetManager::GetInstance().GetRecordBySource(outPath);
		auto path = NE::Resource::ComputeArtifactPathFromUUID(record->id, NE::Resource::ResourceType::Prefab);

		uint32_t localIDs = 0;
		if (m_isScene)
			NE::CreatePrefabFromEntity(EditorScene::s_rootOrder[0], record->id, localIDs, true);
		else
			NE::CreatePrefabFromEntity(EditorScene::s_selection.GetLastDropped(), record->id, localIDs, true);

		Serialization::JSON::SerializePrefab(outPath, isScene);

		return Cook({}, path);
	}

}
