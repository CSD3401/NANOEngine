#include "LUIDGenerator.hpp"

namespace NE::Core {

	std::atomic<uint64_t> LUIDGenerator::counter = 0;

	uint64_t LUIDGenerator::Generate(std::string_view _prefix) {
		uint64_t p0 = _prefix.size() > 0 ? static_cast<uint64_t>(_prefix[0]) & 0xFF : 0;
		uint64_t p1 = _prefix.size() > 1 ? static_cast<uint64_t>(_prefix[1]) & 0xFF : 0;
		uint64_t prefix16 = (p0 << 8) | p1;
		uint64_t time32 = static_cast<uint64_t>(GetTimeInSeconds()) & 0xFFFFFFFF;
		uint64_t count16 = (counter.fetch_add(1) & 0xFFFF);

		return
			(prefix16 << 48) |   // top 16 bits
			(time32 << 16) |   // middle 32 bits
			(count16);           // bottom 16 bits
	}

	uint32_t LUIDGenerator::GetTimeInSeconds() {
		auto now = std::chrono::system_clock::now();
		auto sec = duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
		return static_cast<uint32_t>(sec);
	}

}
