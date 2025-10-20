#include "AssetManager.hpp"
#include <fstream>
#include <filesystem>



#include "UUID.hpp"

namespace {
	std::string ToLower(std::string s) { 
		for (auto& c : s) 
			c = (char)std::tolower((unsigned char)c); 
		return s; 
	}
}

namespace Editor {

	AssetManager& AssetManager::GetInstance() {
		static AssetManager am;
		return am;
	}

	void AssetManager::GenerateMetadata(const std::string& sourcePath) {
		std::filesystem::path fsSourcePath = sourcePath;
		std::filesystem::path metaPath = sourcePath + ".nanometa";

		std::string uuid = GenerateUUID();

		std::ofstream ofs(metaPath);
		ofs << "importerVersion: " << CURRENT_FORMAT_VERSION << '\n'
			<< "uuid: " << uuid << '\n';

		AssetType assetType = GetAssetTypeFromExtension(fsSourcePath.extension().string());

		switch (assetType) {
		case AssetType::Texture: {
			ofs << "assetType: Texture\n"
				<< "sourcePath: " << sourcePath << '\n';
			// cook To be done
			break;
		}
		case AssetType::Mesh: {

			break;
		}
		default:
			break;
		}


		ofs.close();

		AssetMetadata metadata;
		metadata.uuid = uuid;
		metadata.type = assetType;
		metadata.sourcePath = sourcePath;

		m_assets[uuid] = std::move(metadata);
	}

	AssetType AssetManager::GetAssetTypeFromExtension(std::string_view extension) {
		std::string e = ToLower(std::string(extension));
		if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga") return AssetType::Texture;
		else if (e == ".fbx" || e == ".obj") return AssetType::Mesh;
		return AssetType::Unknown;
	}

	bool AssetManager::ImportTexture() {




		return false;
	}

}
