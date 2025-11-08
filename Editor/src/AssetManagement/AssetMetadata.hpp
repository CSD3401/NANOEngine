#ifndef ASSET_METADATA_HPP
#define ASSET_METADATA_HPP

#include <string>

namespace Editor {

	using UUID = std::string;

	enum class AssetType : uint16_t {
		Unknown,
		Texture,
		Mesh,
		Shader,
		Material,
		Audio,
		Prefab
	};

	struct AssetMetadata {
		UUID uuid; // maybe not needed here? remove in future if not needed
		AssetType type;
		std::string sourcePath;
	};

}


#endif
