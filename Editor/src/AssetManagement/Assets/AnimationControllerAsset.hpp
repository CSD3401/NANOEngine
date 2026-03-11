#ifndef ANIMATION_CONTROLLER_ASSET_HPP
#define ANIMATION_CONTROLLER_ASSET_HPP

#include "IAsset.hpp"

namespace Editor::Assets {

	class AnimationControllerAsset final : public IAsset {
	public:
		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;
	};

}

#endif // !IASSET_HPP
