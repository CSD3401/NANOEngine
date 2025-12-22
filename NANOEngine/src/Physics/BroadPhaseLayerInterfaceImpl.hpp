#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

namespace NE::Physics {
    class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        enum : uint8_t { BP_STATIC = 0, BP_DYNAMIC = 1, BP_COUNT = 2 };

        uint32_t GetNumBroadPhaseLayers() const override;

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
            switch (static_cast<uint8_t>(inLayer)) {
            case BP_STATIC:  return "BP_STATIC";
            case BP_DYNAMIC: return "BP_DYNAMIC";
            default:         return "BP_UNKNOWN";
            }
        }
#endif
    };
}

