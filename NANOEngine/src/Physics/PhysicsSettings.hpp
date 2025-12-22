#pragma once
#include <cstdint>

namespace NE::Physics {
    struct PhysicsSettings {
        float fixedDt = 1.0f / 60.0f;
        int collisionSteps = 1;
        float gravityY = -9.81f;

        uint32_t maxBodies = 8192;
        uint32_t maxBodyPairs = 65536;
        uint32_t maxContactConstraints = 10240;
        uint32_t tempAllocatorBytes = 10 * 1024 * 1024;
    };
}
