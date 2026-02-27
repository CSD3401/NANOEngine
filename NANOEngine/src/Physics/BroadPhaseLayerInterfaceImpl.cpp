#include "pch.h"
#include "BroadPhaseLayerInterfaceImpl.hpp"

namespace NE::Physics {
    uint32_t BroadPhaseLayerInterfaceImpl::GetNumBroadPhaseLayers() const { 
        return BP_COUNT; 
    }

    JPH::BroadPhaseLayer 
        BroadPhaseLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
        return (inLayer < 32) ? JPH::BroadPhaseLayer(BP_STATIC)
            : JPH::BroadPhaseLayer(BP_DYNAMIC);
    }
}
