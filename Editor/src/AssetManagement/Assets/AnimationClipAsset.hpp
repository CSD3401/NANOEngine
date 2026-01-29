#ifndef ANIMATION_CLIP_ASSET_HPP
#define ANIMATION_CLIP_ASSET_HPP

#include "IAsset.hpp"

namespace NE::Animation {
	class AnimationClip;
}

namespace Editor::Assets {
	struct AnimClipImportSettings;

	class AnimationClipAsset final : public IAsset {
	public:
		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;

		bool SaveAnimationClip(const std::string& sourcePath,
			const NE::Animation::AnimationClip& clip) const;
	};

}

#endif // !IASSET_HPP
