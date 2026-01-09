#ifndef PREFAB_ASSET_HPP
#define PREFAB_ASSET_HPP

#include "IAsset.hpp"
#include <optional>
#include <string>

namespace Editor::Assets {
	struct PrefabAsset final : public IAsset {
	public:
		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;

		bool SavePrefab(const std::string& outPath);
	};
}
#endif