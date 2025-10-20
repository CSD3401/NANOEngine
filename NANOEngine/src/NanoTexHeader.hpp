#pragma once
#include <cstdint>

namespace NE::Resource {

	constexpr int CURRENT_IMPORTER_VERSION = 1;

#pragma pack(push, 1)
	struct NanoTexHeader {
		uint32_t magic = 0x4E544558;
		uint16_t importerVersion = CURRENT_IMPORTER_VERSION;
		uint32_t width, height;
		uint16_t mipCount, layers;
		uint8_t isSRGB;
		uint8_t shape;
		uint8_t format;
	};
#pragma pack(pop)

}
