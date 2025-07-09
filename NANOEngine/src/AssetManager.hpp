/*********************************************************************
 * \file		AssetManager.hpp
 * \brief		Manages the loading, retrieval, and unloading of
				assets in the application.
 *
 * \author		Yap Zi Yang Irwen, y.ziyangirwen, 2301345
 * \email		y.ziyangirwen@digipen.edu
 * \date		1 September 2024
 *
 * \copyright	Copyright(C) 2024 DigiPen Institute of Technology.
 *				Reproduction or disclosure of this file or its contents without the
 *				prior written consent of DigiPen Institute of Technology is prohibited.
 *********************************************************************/

#ifndef ASSET_MANAGER_HPP
#define ASSET_MANAGER_HPP

#include <string>
#include <unordered_map>

//#include "Utility/MetadataHandler.hpp"
//#include "Logger.hpp"

class Texture;

/**
 * \class AssetManager
 * \brief Manages the loading, retrieval, and unloading of assets in the application.
 *
 * The AssetManager is a singleton class that manages different types of assets (e.g., textures, sounds).
 * It provides methods to load assets from files, retrieve assets, and unload assets.
 */

namespace NANOEngine::Asset {

	class AssetManager {
	public:

		/**
		 * \brief Retrieves the singleton instance of the AssetManager.
		 * \return A reference to the AssetManager instance.
		 */
		static AssetManager& GetInstance() {
			static AssetManager instance;
			return instance;
		}

		/**
		 * \brief Loads an asset from a file and stores it in the asset map.
		 *
		 * \tparam T The type of the asset to load.
		 * \param name The name to associate with the loaded asset.
		 * \param fileName The name of the file to load the asset from.
		 * \return A shared pointer to the loaded asset, or nullptr if the asset failed to load.
		 */
		template <typename T>
		std::shared_ptr<T> Load(const std::string& uuid) {
			auto& map = GetAssetMap<T>();

			if (map.find(uuid) == map.end()) {
				std::shared_ptr<T> asset = std::make_shared<T>();
				std::string filePath = MetadataHandler::RetrieveFilePathFromUUID(uuid);
				if (filePath == "") return nullptr;
				if (!asset->LoadFromFile(filePath)) {
					Logger::Instance().Log(Logger::Level::ERR, "[AssetManager] Failed to load: " + filePath);
					return nullptr;
				}

				map[uuid] = asset;
				return asset;
			}

			return nullptr;
		}

		template <typename T>
		std::shared_ptr<T> Load(const std::string& filePath, bool) {
			auto& map = GetAssetMap<T>();

			if (!MetadataHandler::MetaFileExists(filePath)) {
				MetadataHandler::GenerateMetaFile(filePath);
			}

			std::string uuid = MetadataHandler::ParseUUIDFromFilePath(filePath);

			if (map.find(uuid) == map.end()) {
				std::shared_ptr<T> asset = std::make_shared<T>();
				if (!asset->LoadFromFile(filePath)) {
					Logger::Instance().Log(Logger::Level::ERR, "[AssetManager] Failed to load: " + filePath);
					return nullptr;
				}

				//std::string metaFile = name + ".meta";
				//if (!MetadataHandler::MetaFileExists(name)) {
				//	MetadataHandler::GenerateMetaFile(name);
				//}

				//std::string uuid = MetadataHandler::ParseUUIDFromMeta(metaFile);

				map[uuid] = asset;
				return asset;
			}

			return map.at(uuid);
		}

		/**
		 * \brief Retrieves an asset by name.
		 *
		 * \tparam T The type of the asset to retrieve.
		 * \param name The name of the asset to retrieve.
		 * \return A shared pointer to the asset, or nullptr if the asset is not found.
		 */
		template <typename T>	// Might not need for now
		std::shared_ptr<T> Get(const std::string& name) {
			auto& map = GetAssetMap<T>();
			auto it = map.find(name);
			if (it != map.end())
				return it->second;
			else {
				// try to load, lazy initialising
				return Load<T>(name);
			}
		}

		template <typename T = Texture>
		std::shared_ptr<T> CreateTexture(const std::string& name) {
			std::shared_ptr<T> asset = std::make_shared<T>();
			GetAssetMap<T>()[name] = asset;
			return asset;
		}

		/**
		 * \brief Unloads an asset by name.
		 *
		 * \tparam T The type of the asset to unload.
		 * \param name The name of the asset to unload.
		 */
		template <typename T>
		void Unload(const std::string& name) {
			auto& map = GetAssetMap<T>();
			map.erase(name);
		}

		template <typename T>
		void UnloadAllOfType() {
			GetAssetMap<T>().clear();
		}

	private:
		/**
		 * \brief Private constructor for the singleton pattern.
		 */
		AssetManager() {}

		/**
		 * \brief Retrieves the map of assets for a specific type.
		 *
		 * \tparam T The type of the assets.
		 * \return A reference to the map of assets for the specified type.
		 */
		template <typename T>
		std::unordered_map<std::string, std::shared_ptr<T>>& GetAssetMap() {
			static std::unordered_map<std::string, std::shared_ptr<T>> assetMap;
			return assetMap;
		}
	};
}

#endif // !ASSET_MANAGER_HPP