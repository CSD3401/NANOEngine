#pragma once
#include <string>
#include "../../Core/Reflection.hpp"

namespace NE::ECS::Component {

    struct EntityMeta {

        std::string name;
        std::string prefabID = "";
        uint64_t luid;
        uint64_t prefabInstanceID = 0; 
        uint16_t prefabLocalID;
        uint8_t layer = 0;
        bool isActive = true;
        bool isPrefabRoot = false;

        NE_REFLECT_BEGIN(EntityMeta)
            NE_REFLECT_FIELD(name),
            NE_REFLECT_FIELD(isActive),
            NE_REFLECT_FIELD(luid),
            NE_REFLECT_FIELD(layer),
            NE_REFLECT_FIELD(prefabID),
            NE_REFLECT_FIELD(prefabLocalID),
            NE_REFLECT_FIELD(isPrefabRoot)
        NE_REFLECT_END()
    };

}
