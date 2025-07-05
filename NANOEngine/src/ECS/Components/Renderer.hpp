#pragma once

#include <filesystem>
#include "../../Graphics/Core/Material.hpp"
#include "../../Graphics/Core/Model.hpp"
#include "../../Core/Reflection.hpp"


namespace NANOEngine::ECS::Component {

	struct Renderer {
		//Graphics::Material material;
		// Exposed
		std::filesystem::path modelPath;

		// Internal
		std::shared_ptr<Graphics::Model> model;

		NE_REFLECT_BEGIN(Renderer)
			NE_REFLECT_FIELD(modelPath)
			NE_REFLECT_END()
	};

}
