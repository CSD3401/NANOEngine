#pragma once

#include <string>
#include <vector>

#include <assimp/scene.h>
#include <rapidjson/document.h>

namespace Editor::Assets::ModelAssetInternal {
	void BuildGeneratedPrefab(
		const aiScene* scene,
		const std::string& modelUUID,
		const std::vector<std::string>& materialUUIDByAssimpMat,
		float sceneScale,
		const std::string& rootName,
		rapidjson::Value& outPrefabObj,
		rapidjson::Document::AllocatorType& alloc
	);
}
