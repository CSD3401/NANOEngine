#pragma once

#include "../../Graphics/Core/Material.hpp"
#include "../../Graphics/Core/Model.hpp"
#include "../../Core/Reflection.hpp"


namespace NE::ECS::Component {

	struct Renderer {
		enum ShadowCastMode : uint8_t {
			Off,
			On,
			TwoSided,
			ShadowsOnly
		};
		//Graphics::Material material;
		// Exposed
		std::string modelUUID;
		std::string materialUUID;
		ShadowCastMode shadowCastMode = ShadowCastMode::Off;
		bool receiveShadows = false;

		// Internal
		std::shared_ptr<Graphics::Model> model;
		std::shared_ptr<Graphics::Material> material;

		bool visible = true;
		bool isDirty = false;  // Dirty flag for editor changes

		uint64_t luid;

		NE_REFLECT_BEGIN(Renderer)
			NE_REFLECT_FIELD(modelUUID),
			NE_REFLECT_FIELD(materialUUID),
			NE_REFLECT_FIELD(shadowCastMode),
			NE_REFLECT_FIELD(receiveShadows),
			NE_REFLECT_FIELD_HIDDEN(luid)
		NE_REFLECT_END()
	};

}
