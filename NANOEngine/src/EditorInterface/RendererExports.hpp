#pragma once

#include <cstdint>
#include <string>
#include "../NANOEngineAPI.hpp"

// Forward Decl
namespace NE::ECS::Component {
	struct Renderer;
}

namespace NE::Renderer {

	namespace Query {

	}

	namespace Command {
		NANOENGINE_API void AssignModel(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignMaterial(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignUITexture(uint32_t e, const std::string& textureUUID, const std::string& materialUUID);
	}
}
