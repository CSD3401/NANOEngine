#pragma once

#include <string>

namespace Editor {
	namespace Serialization {
		namespace JSON {
			void SerializeScene(const std::string& path);
			void SerializePrefab(const std::string& path, bool isScene = false);
			bool CookPrefabToBinary(const std::string& jsonPath, const std::string& binPath);
			void SerializeProjectSettings();
			void SerializeUserSettings();
		}
	}

	namespace Deserialization {
		namespace JSON {
			void DeserializeScene(const std::string& path);
			void DeserializeProjectSettings();
			void DeserializeUserSettings();
		}
	}
}

