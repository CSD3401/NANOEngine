#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NFNT_MAGIC = 0x4E464E54; // 'NFNT'
	inline constexpr int CURRENT_NANOFONT_FORMAT_VERSION = 1;

#pragma pack(push, 1)
	struct NanoFontHeader {
		uint32_t magic = NFNT_MAGIC;
		uint16_t version = CURRENT_NANOFONT_FORMAT_VERSION;
		uint16_t reserved1 = 0;
		uint64_t ttfDataSize = 0;

		// Pre-computed metrics at 100pt reference size
		float ascent100 = 0.0f;
		float descent100 = 0.0f;
		float lineHeight100 = 0.0f;

		uint8_t reserved[36]{};
	};
#pragma pack(pop)

	static_assert(sizeof(NanoFontHeader) == 64, "NanoFontHeader must be exactly 64 bytes");

}
