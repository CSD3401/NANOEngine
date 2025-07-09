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
		std::filesystem::path materialPath;

		// Internal
		std::shared_ptr<Graphics::Model> model;
		std::shared_ptr<Graphics::Material> material;

		NE_REFLECT_BEGIN(Renderer)
			NE_REFLECT_FIELD(modelPath),
			NE_REFLECT_FIELD(materialPath)
			NE_REFLECT_END()
	};

}
