#include "pch.h"
#include "ObjectVsBroadPhaseLayerFilterImpl.hpp"

#include "BroadPhaseLayerInterfaceImpl.hpp"

namespace NE::Physics {

    bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer, JPH::BroadPhaseLayer inBP) const {
        if (inLayer == 0)
            return static_cast<uint8_t>(inBP) == BroadPhaseLayerInterfaceImpl::BP_DYNAMIC;

        return true;
    }
}
