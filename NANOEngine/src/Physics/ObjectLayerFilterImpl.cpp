#include "pch.h"
#include "ObjectLayerFilterImpl.hpp"

namespace NE::Physics {
	ObjectLayerFilterImpl::ObjectLayerFilterImpl(Core::LayerMask mask)
		: mLayerMask(mask) {
	}

	bool ObjectLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer) const {
		if (mLayerMask == 0)
			return true; // 0 == "hit all layers"

		const uint32_t baseLayer = static_cast<uint32_t>(inLayer) & 31u; // inLayer % 32
		return (mLayerMask & (1u << baseLayer)) != 0;
	}
}