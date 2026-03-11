#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include <unordered_map>
#include <vector>
#include <array>
#include <memory>

#include "AssetRecord.hpp"
#include "Assets/IAsset.hpp"

namespace Editor::Assets {
	constexpr uint16_t CURRENT_META_SCHEMA_VERSION = 1;

	// Helper: number of asset types (tracks the last enum entry)
	inline constexpr size_t AssetTypeCount =
		static_cast<size_t>(AssetType::Lighting) + 1;

	class AssetManager {
	public:
		static AssetManager& GetInstance();

		void GenerateMetadata(const std::string& sourcePath, std::string uuid = "");
		void ReimportAsset(const std::string& sourcePath);
		void CleanupOrphanArtifacts();

		std::string RetrieveUUID(const std::string& sourcePath);
		std::string RetrieveFilename(const std::string& uuid);

		AssetRecord* GetRecord(const UUID& id);
		const AssetRecord* GetRecord(const UUID& id) const;

		AssetRecord* GetRecordBySource(const std::string& sourcePath);
		const AssetRecord* GetRecordBySource(const std::string& sourcePath) const;

		const std::vector<std::pair<std::string, UUID>>&
			GetAssetsOfType(AssetType type) const;
	private:
		AssetManager();
		~AssetManager() = default;

		AssetRecord& RegisterAsset(
			const UUID& id,
			AssetType type,
			const std::filesystem::path& sourcePath);

		void UnregisterAsset(const UUID& id);

		AssetType GetAssetTypeFromString(std::string_view extension);
		AssetType GetAssetTypeFromExtension(std::string_view extension);

		std::unordered_map<UUID, AssetRecord> m_assetsByID; // uuid to assetrecord
		std::unordered_map<std::string, UUID> m_idByPath; // sourcepath to uuid

		using NameUUIDList = std::vector<std::pair<std::string, UUID>>; // filename to uuid
		std::array<NameUUIDList, AssetTypeCount> m_assetsByType;
	};
}

#endif
