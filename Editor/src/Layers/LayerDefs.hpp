#pragma once
#include <string>

#include <Core/Layers.hpp>

namespace Editor::Layers {

    struct LayerSlot {
        bool used = false;
        std::string name;
    };

    struct LayerDatabaseData {
        LayerSlot slots[NE::Core::MAX_USER_LAYERS];
        NE::Core::LayerMask collideWith[NE::Core::MAX_USER_LAYERS]{};
    };

}
