#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NTEX_MAGIC = 0x4E544558;
	inline constexpr int CURRENT_NANOTEX_FORMAT_VERSION = 1;

#pragma pack(push, 1)
	struct NanoTexHeader {
		uint32_t magic = NTEX_MAGIC;
		uint16_t importerVersion = CURRENT_NANOTEX_FORMAT_VERSION;
		uint32_t width, height;
		uint16_t mipCount, layers;
		uint8_t isSRGB;
		uint8_t shape;
		uint8_t format;
	};
#pragma pack(pop)

}
