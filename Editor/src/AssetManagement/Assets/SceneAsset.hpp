#ifndef SCENE_ASSET_HPP
#define SCENE_ASSET_HPP

#include "IAsset.hpp"

namespace Editor::Assets {

	class SceneAsset final : public IAsset {
		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;
	};

}

#endif // !IASSET_HPP
