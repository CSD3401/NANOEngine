#pragma once
#include <vector>
#include "EditorEntity.hpp"
//#include "IECSBridge.hpp"   // C-API wrapper into your ECS.dll

namespace Editor {

    class EditorScene {
    public:

    //private:
        //IECSBridge& m_bridge;
        static std::vector<EditorEntity> s_entities;
        static EditorEntity* s_selectedEntity;
    };

}
