#pragma once
#include <Jolt/Jolt.h>
#include "Core/Layers.hpp"

namespace NE::Physics {

	class LayerMaskBodyFilter : public JPH::ObjectLayerFilter {
    public:
        explicit LayerMaskBodyFilter(NE::Core::Layers::Mask mask)
            : mLayerMask(mask) {
        }

        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            if (mLayerMask == 0)
                return true; // 0 == "hit all layers"
            return (mLayerMask & (1u << static_cast<uint32_t>(inLayer))) != 0;
        }

    private:
        NE::Core::Layers::Mask mLayerMask;
	};

}
