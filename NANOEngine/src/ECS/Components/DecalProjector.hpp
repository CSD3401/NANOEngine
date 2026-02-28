#pragma once

#include "Math/Vec2.hpp"
#include "Graphics/Core/Material.hpp"
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

	struct DecalProjector {
		std::string materialUUID;
		std::shared_ptr<Graphics::Material> material;
		Math::Vec3 pivot{ 0.0f, 0.0f, -0.5f };
		Math::Vec2 tilling{ 1.0f, 1.0f };
		Math::Vec2 offset{ 0.0f, 0.0f };
		float width = 1.0f;
		float height = 1.0f;
		float depth = 1.0f;
		float opacity = 1.0f;
		float drawDistance = 100.0f;
		float startFadeDistance = 80.0f;
		uint64_t luid = 0;

		NE_REFLECT_BEGIN(DecalProjector)
			NE_REFLECT_FIELD_NAMED(width, "Width"),
			NE_REFLECT_FIELD_NAMED(height, "Height"),
			NE_REFLECT_FIELD_NAMED(depth, "Projection Depth"),
			NE_REFLECT_FIELD_NAMED(pivot, "Pivot"),
			NE_REFLECT_FIELD_HIDDEN(materialUUID, "Material"),
			NE_REFLECT_FIELD_NAMED(tilling, "Tilling"),
			NE_REFLECT_FIELD_NAMED(offset, "Offset"),
			NE_REFLECT_FIELD_NAMED(opacity, "Opacity"),
			NE_REFLECT_FIELD_NAMED(drawDistance, "Draw Distance"),
			NE_REFLECT_FIELD_NAMED(startFadeDistance, "Start Fade"),
			NE_REFLECT_FIELD_HIDDEN(luid)
		NE_REFLECT_END()
	};
}
