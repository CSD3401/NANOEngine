#pragma once
#include <string>

namespace Editor {

    struct EditorEntity {
        uint32_t linkedEntity;
        std::string displayName = "Unnamed Entity";
        bool isActive = true;
    };

}