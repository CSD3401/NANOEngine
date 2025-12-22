#include "ObjectLayerPairFilterImpl.hpp"

namespace NE::Physics {
	ObjectLayerPairFilterImpl::ObjectLayerPairFilterImpl(const std::array<NE::Core::LayerMask, NE::Core::MAX_LAYERS>& matrix)
		: m_matrix(matrix) {
	}

	bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const {
		if (a >= NE::Core::MAX_LAYERS || b >= NE::Core::MAX_LAYERS)
			return false;

		uint8_t rawA = static_cast<uint8_t>(a % 32);
		uint8_t rawB = static_cast<uint8_t>(b % 32);

		const auto mask = m_matrix[(size_t)rawA];
		return (mask & NE::Core::LayerBit((NE::Core::LayerID)rawB)) != 0;
	}
}

