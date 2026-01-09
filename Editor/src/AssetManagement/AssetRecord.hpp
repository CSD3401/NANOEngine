#ifndef ASSET_RECORD_HPP
#define ASSET_RECORD_HPP

#include <filesystem>
#include <cstdint>
#include <memory>

#include "UUID.hpp"
#include "Assets/IAsset.hpp"

namespace Editor::Assets {

	enum class AssetType : uint16_t {
		Unknown,
		Texture,
		Model,
		Shader,
		Material,
		Audio,
		Prefab,
		Scene,
		Folder
	};

	struct AssetRecord {
		bool isLoaded = false;
		AssetType type;
		UUID id;
		std::filesystem::path sourcePath;
		std::unique_ptr<IAsset> asset;
	};

}

#endif // !ASSET_RECORD_HPP
