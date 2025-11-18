#pragma once

#include "../../Graphics/Core/Material.hpp"
#include "../../Graphics/Core/Model.hpp"
#include "../../Core/Reflection.hpp"


namespace NE::ECS::Component {

	struct Renderer {
		//Graphics::Material material;
		// Exposed
		std::string modelUUID;
		std::string materialUUID;

		// Internal
		std::shared_ptr<Graphics::Model> model;
		std::shared_ptr<Graphics::Material> material;

		bool visible = true;

		uint64_t luid;

		NE_REFLECT_BEGIN(Renderer)
			NE_REFLECT_FIELD(modelUUID),
			NE_REFLECT_FIELD(materialUUID)
		NE_REFLECT_END()
	};

}
