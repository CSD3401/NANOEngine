#pragma once

#include <string>

namespace Editor {
	namespace Serialization {
		namespace JSON {
			void SerializeScene(const std::string& path);
			void SerializePrefab(const std::string& path, bool isScene = false);
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

