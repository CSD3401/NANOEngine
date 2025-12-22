#include "ObjectLayerFilterImpl.hpp"

namespace NE::Physics {
	ObjectLayerFilterImpl::ObjectLayerFilterImpl(Core::LayerMask mask)
		: mLayerMask(mask) {
	}

	bool ObjectLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer) const {
		if (mLayerMask == 0)
			return true; // 0 == "hit all layers"
		return (mLayerMask & (1u << static_cast<uint32_t>(inLayer))) != 0;
	}
}