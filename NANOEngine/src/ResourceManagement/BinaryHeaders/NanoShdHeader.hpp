#pragma once
#include <cstdint>

namespace NE::Resource {

	inline constexpr uint32_t NSHD_MAGIC = 0x4E534844;
	inline constexpr int CURRENT_NANOSHD_FORMAT_VERSION = 1;

#pragma pack(push, 1)
	struct NanoShdHeader {
		uint32_t magic = NSHD_MAGIC;
		uint16_t importerVersion = CURRENT_NANOSHD_FORMAT_VERSION;
		uint32_t stagesMask = 0; // VS=1<<0, FS=1<<4 etc...
		uint8_t programFormat = 1; // 1 = GL program binary

		uint64_t sourceHash = 0;
		uint64_t definesHash = 0;
		uint64_t permutationKey = 0;

		uint32_t programBinaryFormat = 0;
		uint32_t programFlags = 0;

		uint64_t programOffset = 0;
		uint64_t programSize = 0;

		uint64_t reflOffset = 0;
		uint64_t reflSize = 0;

		uint32_t endianMarker = 0x01020304;
		uint32_t headerSize = sizeof(NanoShdHeader);
	};
#pragma pack(pop)

}
