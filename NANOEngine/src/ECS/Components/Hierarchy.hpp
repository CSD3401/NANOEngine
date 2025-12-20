#pragma once
#include <vector>
#include "Core/Reflection.hpp"

namespace NE::ECS::Component {

    inline constexpr uint32_t INVALID_PARENT = UINT32_MAX;

    struct Hierarchy {
        uint32_t parent = INVALID_PARENT;
        std::vector<uint32_t> children{};

        uint64_t luid = 0;
        uint64_t parentLuid = 0;

        NE_REFLECT_BEGIN(Hierarchy)
            NE_REFLECT_FIELD(luid),
            NE_REFLECT_FIELD(parentLuid)
        NE_REFLECT_END()
    };

}
