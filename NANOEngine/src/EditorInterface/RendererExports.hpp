#pragma once

#include <cstdint>
#include <string>
#include "../NANOEngineAPI.hpp"

// Forward Decl
namespace NE::ECS::Component {
	struct Renderer;
}

namespace NE::Graphics {
	struct RenderSettings;
}

namespace NE::Renderer {

	namespace Query {
		NANOENGINE_API std::string GetModel(uint32_t e);
		NANOENGINE_API std::string GetMaterial(uint32_t e);
	}

	namespace Command {
		NANOENGINE_API void AssignModel(uint32_t e, const std::string& uuid);
		NANOENGINE_API void AssignMaterial(uint32_t e, const std::string& uuid);
		NANOENGINE_API Graphics::RenderSettings& GetRenderSettings();
	}

}
