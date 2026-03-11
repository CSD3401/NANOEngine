#include "pch.h"
#include "PrefabAsset.hpp"

#include <Engine.hpp>
#include <ResourceManagement/ResourcePaths.hpp>

// Components
#include <ECS/Components/EntityMeta.hpp>
#include <ECS/Components/Transform.hpp>
#include <ECS/Components/Renderer.hpp>
#include <ECS/Components/Light.hpp>
#include <ECS/Components/Collider.hpp>
#include <ECS/Components/Rigidbody.hpp>
#include <ECS/Components/NativeScript.hpp>
#include <ECS/Components/Camera.hpp>
#include <ECS/Components/UIRectTransform.hpp>
#include <ECS/Components/UICanvas.hpp>
#include <ECS/Components/UIImage.hpp>
#include <ECS/Components/Hierarchy.hpp>
#include <ECS/Components/PrefabLink.hpp>
#include <ECS/Components/PrefabInstance.hpp>
#include <ECS/Components/CharacterController.hpp>

#include "../../EditorScene.hpp"
#include "../../Serialization/Serializer.hpp"
#include "../AssetManager.hpp"


namespace Editor::Assets {
	bool PrefabAsset::Cook(const std::string& sourcePath,
		const std::string& outPath) const {

		return Serialization::JSON::CookPrefabToBinary(sourcePath, outPath);
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

		return Cook(outPath, path);
	}

}
