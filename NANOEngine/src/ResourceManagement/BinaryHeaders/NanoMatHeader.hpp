#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NTEX_MAGIC = 0x4E544558;
	inline constexpr int CURRENT_NANOMAT_FORMAT_VERSION = 1;

#pragma pack(push, 1)
	struct NanoTexHeader {
		uint32_t magic = NTEX_MAGIC;
	};
#pragma pack(pop)

}
