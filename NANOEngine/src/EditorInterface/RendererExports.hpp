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
		NANOENGINE_API void AssignRendererModel(NE::ECS::Component::Renderer& r, std::string filepath);
		NANOENGINE_API void AssignRendererMaterial(NE::ECS::Component::Renderer& r, std::string filepath);
	}

}
