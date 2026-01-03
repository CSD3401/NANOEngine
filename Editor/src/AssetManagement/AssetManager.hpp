#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP
#include <unordered_map>
#include <vector>
#include "AssetMetadata.hpp"
#include "Settings/TextureImportSettings.hpp"

namespace Editor {

	constexpr uint16_t CURRENT_META_SCHEMA_VERSION = 1;

	class AssetManager {
	public:
		static AssetManager& GetInstance();

		void LoadAssetRegistry();
		void SaveAssetRegistry();

		void GenerateMetadata(const std::string& sourcePath);
		void ReimportAsset(const std::string& sourcePath);

		std::string RetrieveUUID(const std::string& sourcePath);
		std::string RetrieveFileName(const std::string& uuid);
		
		bool SaveTextureImportSettings(const std::string& metaPath, const TextureImportSettings& settings);

		template <AssetType T>
		std::vector<std::pair<std::string, UUID>>& GetAssetsOfType() {
			static std::vector<std::pair<std::string, UUID>> registry;
			return registry;
		}
	private:
		AssetManager();
		~AssetManager() = default;

		AssetType GetAssetTypeFromString(std::string_view extension);
		AssetType GetAssetTypeFromExtension(std::string_view);

		bool CookTexture(const std::string& sourcePath, const std::string& outPath, const TextureImportSettings& settings);
		bool CookShader(const std::string& sourcePath, const std::string& outPath);
		bool CookMaterial(const std::string& sourcePath, const std::string& outPath);
		bool CookMesh(const std::string& sourcePath, const std::string& outPath);
		bool CookFont(const std::string& sourcePath, const std::string& outPath);

		std::unordered_map<UUID, AssetMetadata> m_assets;
	};

}

#endif
