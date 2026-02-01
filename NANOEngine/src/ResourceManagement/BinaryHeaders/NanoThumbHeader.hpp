#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NTHM_MAGIC = 0x4E54484D;
	inline constexpr uint16_t CURRENT_NANOTHUMB_FORMAT_VERSION = 1;

	enum class ThumbFormat : uint8_t {
		BC3 = 0,
	};

#pragma pack(push, 1)
	struct NanoThumbHeader {
		uint32_t magic = NTHM_MAGIC;
		uint16_t version = CURRENT_NANOTHUMB_FORMAT_VERSION;
		uint16_t width = 256;
		uint16_t height = 256;
		uint16_t mipCount = 1;
		ThumbFormat format = ThumbFormat::BC3;
		uint32_t dataSizeBytes = 0;
	};
#pragma pack(pop)

}