#pragma once
#include <cstdint>

namespace NE::Resource {

	enum class ResourceType : uint16_t {
		Unknown,
		Texture,
		Model,
		Shader,
		Material,
		Audio,
		Prefab,
		Scene
	};

}
