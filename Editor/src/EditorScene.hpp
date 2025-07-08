#pragma once
#include <vector>
#include <string>
#include "EditorEntity.hpp"

namespace Editor {

    class EditorScene {
    public:
        static std::vector<EditorEntity> s_entities;
        static EditorEntity* s_selectedEntity;

        static std::string currentScenePath;
    };

}
