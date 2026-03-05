#pragma once

#include <string>
#include <vector>

#include <assimp/scene.h>
#include <rapidjson/document.h>

#include <Math/Vec3.hpp>

#include "../ModelAsset.hpp"

namespace Editor::Assets::ModelAssetInternal {
	bool LoadMetaDocumentForCook(const std::string& metaPath, rapidjson::Document& outDoc);
	bool LoadMetaDocumentStrict(const std::string& metaPath, rapidjson::Document& outDoc);
	bool SaveMetaDocument(const std::string& metaPath, rapidjson::Document& doc);

	void WriteSubmeshesToMeta(
		rapidjson::Document& doc,
		const aiScene* scene,
		const std::vector<NE::Math::Vec3>& submeshPivots
	);

	void BuildSubmeshCacheFromScene(
		const aiScene* scene,
		std::vector<ModelAsset::SubmeshEntry>& outSubmeshes
	);

	void UpsertGeneratedPrefab(
		rapidjson::Document& doc,
		rapidjson::Value& generatedPrefab
	);

	void ParseSubmeshes(
		const rapidjson::Value& arr,
		std::vector<ModelAsset::SubmeshEntry>& outSubmeshes
	);
}
