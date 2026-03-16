#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NLGT_MAGIC = 0x4E4C4754;
	inline constexpr uint16_t CURRENT_NANOLIGHTING_FORMAT_VERSION = 2;

#pragma pack(push, 1)
	struct NanoLightingHeader {
		uint32_t magic = NLGT_MAGIC;
		uint16_t version = CURRENT_NANOLIGHTING_FORMAT_VERSION;
		uint64_t payloadBytes = 0;
	};
#pragma pack(pop)

}
