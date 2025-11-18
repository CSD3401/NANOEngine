#pragma once
#include "../Core/Entity.hpp"

namespace NE::ECS::Component {
    struct Parent {
        Entity parent = NO_ENTITY; // NO_ENTITY = root
    };
}
