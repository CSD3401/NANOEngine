#pragma once
#include <vector>
#include "EditorEntity.hpp"

namespace Editor {

    class EditorScene {
    public:
        static std::vector<EditorEntity> s_entities;
        static EditorEntity* s_selectedEntity;
    };

}
