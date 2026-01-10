#ifndef MODEL_ASSET_HPP
#define MODEL_ASSET_HPP

#include "IAsset.hpp"

#include <optional>

#include "../Settings/ModelImportSettings.hpp"

namespace Editor::Assets {
	class ModelAsset final : public IAsset {
	public:
		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;

		ModelImportSettings& GetImportSettings();

	private:
		std::optional<ModelImportSettings> importSettings;
	};
}

#endif // !MODEL_ASSET_HPP