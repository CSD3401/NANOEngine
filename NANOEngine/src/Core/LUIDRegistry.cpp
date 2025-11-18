#include "LUIDRegistry.hpp"

namespace NE::Core {

	void LUIDRegistry::Register(uint64_t _id, void* _ptr, uint32_t _entityOwner, LuidType _type) {
		m_map[_id] = { _ptr, _entityOwner, _type };
	}

	void LUIDRegistry::Unregister(uint64_t _id) {
		m_map.erase(_id);
	}

	const LuidRecord* LUIDRegistry::Find(uint64_t _id) const {
		auto it = m_map.find(_id);
		return (it != m_map.end()) ? &it->second : nullptr;
	}

	void LUIDRegistry::Clear() {
		m_map.clear();
	}

}
