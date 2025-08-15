#pragma once
#include <string>
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct EntityMeta {

        std::string name;
        std::string guid;

        NE_REFLECT_BEGIN(EntityMeta)
            NE_REFLECT_FIELD(name)
        NE_REFLECT_END()
    };

}
