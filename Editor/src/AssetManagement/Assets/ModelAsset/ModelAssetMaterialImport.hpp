#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <assimp/scene.h>

namespace Editor::Assets::ModelAssetInternal {
	struct MaterialImportResult {
		std::vector<std::string> materialUUIDByAssimpMat;
	};

	bool ImportAssimpMaterials(
		const aiScene* scene,
		const std::filesystem::path& sourceDir,
		MaterialImportResult& outResult
	);
}
