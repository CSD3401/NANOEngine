#pragma once

#include <cstdint>
#include <limits>
#include <string>

#include "Core/Reflection.hpp"
#include "Math/Vec2.hpp"

namespace NE::ECS::Component {

	inline constexpr uint32_t INVALID_LIGHTMAP_PAGE_SLOT = std::numeric_limits<uint32_t>::max();

	struct LightmapBinding {
		bool enabled = false;
		std::string pageId;
		Math::Vec2 uvScale = { 1.0f, 1.0f };
		Math::Vec2 uvOffset = { 0.0f, 0.0f };

		// Runtime-only resolution cache. These fields are intentionally not reflected.
		uint32_t resolvedPageSlot = INVALID_LIGHTMAP_PAGE_SLOT;
		bool pageResolved = false;

		NE_REFLECT_BEGIN(LightmapBinding)
			NE_REFLECT_FIELD(enabled),
			NE_REFLECT_FIELD(pageId),
			NE_REFLECT_FIELD(uvScale),
			NE_REFLECT_FIELD(uvOffset)
		NE_REFLECT_END()
	};

}
