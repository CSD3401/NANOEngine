#include "AssetManager.hpp"

namespace NE::Asset {
	AssetManager& AssetManager::GetInstance() {
		static AssetManager instance;
		return instance;
	}
}