#ifndef LUID_REGISTRY_HPP
#define LUID_REGISTRY_HPP

#include <cstdint>
#include <unordered_map>

namespace NE::Core {

	enum class LuidType : uint8_t {

	};

	struct LuidRecord {
		void* m_ptr;
		uint32_t m_entityOwner;
		LuidType m_type;
	};

	class LUIDRegistry {
	public:
		void Register(uint64_t, void*, uint32_t, LuidType);
		void Unregister(uint64_t);
		const LuidRecord* Find(uint64_t) const;
		void Clear();

	private:
		std::unordered_map<uint64_t, LuidRecord> m_map;
	};

}

#endif
