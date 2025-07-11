#include "AssetManager.hpp"

namespace NANOEngine::Asset {
	AssetManager& AssetManager::GetInstance() {
		static AssetManager instance;
		return instance;
	}
}