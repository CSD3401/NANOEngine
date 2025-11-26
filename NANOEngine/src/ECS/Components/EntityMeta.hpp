#pragma once
#include <string>
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct EntityMeta {

        std::string name;
        
        uint64_t luid;
        bool isActive = true;  // Entity active state - controls whether entity updates and renders
        //std::string guid; // commented out as unused param warning - RF during m1

        NE_REFLECT_BEGIN(EntityMeta)
            NE_REFLECT_FIELD(name),
            NE_REFLECT_FIELD(luid),
            NE_REFLECT_FIELD(isActive)
        NE_REFLECT_END()
    };

}
