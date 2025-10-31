#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP
#include <unordered_map>
#include <vector>
#include "AssetMetadata.hpp"

namespace Editor {

	constexpr uint16_t CURRENT_META_SCHEMA_VERSION = 1;

	class AssetManager {
	public:
		static AssetManager& GetInstance();

		void LoadAssetRegistry();
		void SaveAssetRegistry();

		void GenerateMetadata(const std::string& sourcePath);

		


	private:
		AssetManager() = default;
		~AssetManager() = default;
		
		AssetType GetAssetTypeFromString(std::string_view extension);
		AssetType GetAssetTypeFromExtension(std::string_view);

		bool ImportTexture();
		bool CookTexture(const std::string& sourcePath, const std::string& outPath);
		bool CookShader(const std::string& sourcePath, const std::string& outPath);
		bool CookMaterial(const std::string& sourcePath, const std::string& outPath);
		bool CookMesh(const std::string& sourcePath, const std::string& outPath);

		std::unordered_map<UUID, AssetMetadata> m_assets;

		//template <typename AssetT>
		//std::vector<std::pair<UUID, std::string>>& GetAssetRegistry() {
		//	static std::vector<std::pair<UUID, std::string>> assetRegistry;
		//	return assetRegistry;
		//}
	};

}

#endif
