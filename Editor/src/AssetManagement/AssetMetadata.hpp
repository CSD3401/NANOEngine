#ifndef ASSET_METADATA_HPP
#define ASSET_METADATA_HPP

#include <string>

namespace Editor {

	using UUID = std::string;

	enum class AssetType : uint16_t {
		Texture,
		Mesh,
		Material,
		Audio,
		Prefab,
		Unknown
	};

	struct AssetMetadata {
		UUID uuid;
		AssetType type;
		std::string sourcePath;
	};

}


#endif
