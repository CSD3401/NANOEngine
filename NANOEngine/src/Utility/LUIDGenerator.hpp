#ifndef LUID_GENERATOR_HPP
#define LUID_GENERATOR_HPP

#include <cstdint>
#include <chrono>
#include <atomic>

namespace NE::Utility {

	class LUIDGenerator {
	public:
		static uint64_t Generate(std::string_view);

	private:
		static uint32_t GetTimeInSeconds();

		static std::atomic<uint64_t> counter;
	};

}

#endif
