#pragma once
#include <cstdint>
#include <initializer_list>

namespace NE::Core {
	using LayerID = uint8_t;
	using LayerMask = uint64_t;

	constexpr LayerID DEFAULT_LAYER = 0xFF;
	constexpr size_t MAX_LAYERS = 64;

	constexpr LayerMask LayerBit(LayerID id) noexcept {
		return LayerMask{ 1 } << id;
	}
}