#pragma once

#include <string>

namespace Editor {
	namespace Serialization {
		namespace JSON {
			void SerializeScene(const std::string& path);
			void SerializePrefab(const std::string& path);
			void SerializeEditorSettings();
		}
	}

	namespace Deserialization {
		namespace JSON {
			void DeserializeEditorSettings();
		}
	}
}

