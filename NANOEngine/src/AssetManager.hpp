#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <memory>

#include "Utility/MetadataHandler.hpp"
#include "NANOEngineAPI.hpp"

namespace NANOEngine::Asset {

	class NANOENGINE_API AssetManager {
	public:

		static AssetManager& GetInstance();

		template <typename T>
		std::shared_ptr<T> Load(const std::string& uuid) {
			auto& map = GetAssetMap<T>();

			//if (map.find(uuid) == map.end()) {
			//	std::shared_ptr<T> asset = std::make_shared<T>();
			//	std::string filePath = Utility::MetadataHandler::RetrieveFilePathFromUUID(uuid);
			//	if (filePath == "") return nullptr;
			//	if (!asset->LoadFromFile(filePath)) {
			//		//Logger::Instance().Log(Logger::Level::ERR, "[AssetManager] Failed to load: " + filePath);
			//		return nullptr;
			//	}

			//	map[uuid] = asset;
			//	return asset;
			//}

			return map.at(uuid);
		}

		template <typename T>
		std::shared_ptr<T> Load(const std::string& filePath, bool) {
			auto& map = GetAssetMap<T>();

			if (!Utility::MetadataHandler::MetaFileExists(filePath)) {
				Utility::MetadataHandler::GenerateMetaFile(filePath);
			}

			std::string uuid = Utility::MetadataHandler::ParseUUIDFromFilePath(filePath);

			if (map.find(uuid) == map.end()) {
				std::shared_ptr<T> asset = std::make_shared<T>();
				if (!asset->LoadFromFile(filePath)) {
					//Logger::Instance().Log(Logger::Level::ERR, "[AssetManager] Failed to load: " + filePath);
					return nullptr;
				}
				asset->uuid = uuid;
				asset->filePath = filePath;

				//std::string metaFile = name + ".meta";
				//if (!MetadataHandler::MetaFileExists(name)) {
				//	MetadataHandler::GenerateMetaFile(name);
				//}

				//std::string uuid = MetadataHandler::ParseUUIDFromMeta(metaFile);

				map[uuid] = asset;
				GetAssetList<T>().emplace_back(filePath, asset);
				return asset;
			}

			return map.at(uuid);
		}

		template <typename T>	// Might not need for now
		std::shared_ptr<T> Get(const std::string& name) {
			auto& map = GetAssetMap<T>();
			auto it = map.find(name);
			if (it != map.end())
				return it->second;
			else {
				// try to load, lazy initialising
				return Load<T>(name, false);
			}
		}

		template <typename T>
		void AddToMap(std::shared_ptr<T> asset, std::string identifier) {
			auto& map = GetAssetMap<T>();
			map[identifier] = asset;
			GetAssetList<T>().emplace_back(identifier, asset);
		}

		//template <typename T = Texture>
		//std::shared_ptr<T> CreateTexture(const std::string& name) {
		//	std::shared_ptr<T> asset = std::make_shared<T>();
		//	GetAssetMap<T>()[name] = asset;
		//	return asset;
		//}

		template <typename T>
		void Unload(const std::string& name) {
			auto& map = GetAssetMap<T>();
			map.erase(name);
		}

		template <typename T>
		void UnloadAllOfType() {
			GetAssetMap<T>().clear();
		}

		template <typename T>
		const std::vector<std::pair<std::string, std::shared_ptr<T>>>& GetAssetsOfType() const {
			return const_cast<AssetManager*>(this)->GetAssetList<T>();
		}

	private:
		AssetManager() {}

		template <typename T>
		std::unordered_map<std::string, std::shared_ptr<T>>& GetAssetMap() {
			static std::unordered_map<std::string, std::shared_ptr<T>> assetMap;
			return assetMap;
		}

		template <typename T>
		std::vector<std::pair<std::string, std::shared_ptr<T>>>& GetAssetList() {
			static std::vector<std::pair<std::string, std::shared_ptr<T>>> assetList;
			return assetList;
		}
	};
}

#endif // !ASSET_MANAGER_HPP