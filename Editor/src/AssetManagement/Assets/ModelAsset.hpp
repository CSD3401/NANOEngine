#ifndef MODEL_ASSET_HPP
#define MODEL_ASSET_HPP

#include "IAsset.hpp"

#include <optional>
#include <string>
#include <vector>
#include <rapidjson/document.h>

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
		void ParseSubmeshes(const rapidjson::Value& arr);

		struct SubmeshEntry {
			std::string name;
			int32_t index;
		};

		mutable std::string m_uuid;
		mutable std::vector<SubmeshEntry> m_submeshes;
		std::optional<ModelImportSettings> importSettings;
	};
}

#endif // !MODEL_ASSET_HPP