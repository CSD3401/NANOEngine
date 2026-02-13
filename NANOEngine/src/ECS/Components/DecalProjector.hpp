#pragma once

#include "Math/Vec2.hpp"
#include "Graphics/Core/Material.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

	struct DecalProjector {
		std::string materialUUID;
		std::shared_ptr<Graphics::Material> material;
		Math::Vec3 pivot;
		Math::Vec2 tilling;
		Math::Vec2 offset;
		float width;
		float height;
		float depth;
		float opacity;
		float drawDistance;
		float startFadeDistance;
		uint64_t luid;

		NE_REFLECT_BEGIN(DecalProjector)
			NE_REFLECT_FIELD_NAMED(width, "Width"),
			NE_REFLECT_FIELD_NAMED(height, "Height"),
			NE_REFLECT_FIELD_NAMED(depth, "Projection Depth"),
			NE_REFLECT_FIELD_NAMED(pivot, "Pivot"),
			NE_REFLECT_FIELD_NAMED(materialUUID, "Material"),
			NE_REFLECT_FIELD_NAMED(tilling, "Tilling"),
			NE_REFLECT_FIELD_NAMED(offset, "Offset"),
			NE_REFLECT_FIELD_NAMED(opacity, "Opacity"),
			NE_REFLECT_FIELD_NAMED(drawDistance, "Draw Distance"),
			NE_REFLECT_FIELD_NAMED(startFadeDistance, "Start Fade"),
			NE_REFLECT_FIELD_HIDDEN(luid)
		NE_REFLECT_END()
	};
}