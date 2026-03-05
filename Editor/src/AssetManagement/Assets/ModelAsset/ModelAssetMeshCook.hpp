#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <assimp/scene.h>

#include <Math/Vec3.hpp>
#include <ResourceManagement/BinaryHeaders/NanoModelHeader.hpp>

#include "../../Settings/ModelImportSettings.hpp"

namespace Editor::Assets::ModelAssetInternal {
	struct MeshCookResult {
		std::vector<NE::Resource::NanoSubmeshDesc> subdescs;
		std::vector<NE::Math::Vec3> submeshPivots;
		std::vector<uint32_t> colliderDataSizes;
	};

	bool CookModelBinary(
		const aiScene* scene,
		const ModelImportSettings& importSettings,
		float sceneScale,
		const std::string& sourcePath,
		const std::filesystem::path& outPath,
		MeshCookResult& outResult
	);
}
