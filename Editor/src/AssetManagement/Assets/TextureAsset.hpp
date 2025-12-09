#ifndef TEXTURE_ASSET_HPP
#define TEXTURE_ASSET_HPP

#include "IAsset.hpp"

#include <optional>

#include "../Settings/TextureImportSettings.hpp"

namespace Editor::Assets {
	struct TextureAsset final : public IAsset {
	public:
		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;

		TextureImportSettings& GetImportSettings(const std::string& sourcePath) const;

	private:
		mutable std::optional<TextureImportSettings> importSettings;
	};
}

#endif // !TEXTURE_ASSET_HPP