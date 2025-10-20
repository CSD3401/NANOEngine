#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP
#include <unordered_map>
#include <vector>
#include "AssetMetadata.hpp"

namespace Editor {

constexpr uint32_t CURRENT_FORMAT_VERSION = 1;

	class AssetManager {
	public:
		static AssetManager& GetInstance();

		void LoadAssetRegistry();
		void SaveAssetRegistry();

		void GenerateMetadata(const std::string& sourcePath);
		


	private:
		AssetManager() = default;
		~AssetManager() = default;
		
		AssetType GetAssetTypeFromExtension(std::string_view);

		bool ImportTexture();
		bool CookTexture(const std::string& sourcePath);

		std::unordered_map<UUID, AssetMetadata> m_assets;

		//template <typename AssetT>
		//std::vector<std::pair<UUID, std::string>>& GetAssetRegistry() {
		//	static std::vector<std::pair<UUID, std::string>> assetRegistry;
		//	return assetRegistry;
		//}
	};

}

#endif
