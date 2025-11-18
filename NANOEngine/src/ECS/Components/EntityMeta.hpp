#pragma once
#include <string>
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct EntityMeta {

        std::string name;
        
        uint64_t luid;

        NE_REFLECT_BEGIN(EntityMeta)
            NE_REFLECT_FIELD(name)
        NE_REFLECT_END()
    };

}
