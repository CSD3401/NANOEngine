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
		struct SubmeshEntry {
			std::string name;
			int32_t index;
		};

		bool Cook(const std::string& sourcePath,
			const std::string& outPath) const override;

		bool LoadImportSettings(const std::string& sourcePath) override;
		bool SaveImportSettings(const std::string& sourcePath) override;

		ModelImportSettings& GetImportSettings();

		std::vector<SubmeshEntry>& GetSubmeshes();
	private:
		void ParseSubmeshes(const rapidjson::Value& arr);

		mutable std::string m_uuid;
		mutable std::vector<SubmeshEntry> m_submeshes;
		mutable std::optional<ModelImportSettings> importSettings;
	};
}

#endif // !MODEL_ASSET_HPP
