#include "ResourceManager.hpp"

namespace NE::Resource {

	ResourceManager& ResourceManager::GetInstance() {
		static ResourceManager instance;
		return instance;
	}

}