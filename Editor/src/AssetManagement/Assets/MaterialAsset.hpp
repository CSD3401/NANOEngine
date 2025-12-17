#ifndef MATERIAL_ASSET_HPP
#define MATERIAL_ASSET_HPP

#include "IAsset.hpp"

namespace Editor::Assets {

	class MaterialAsset final : public IAsset {
	public:
		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;
	};

}

#endif // !MATERIAL_ASSET_HPP
