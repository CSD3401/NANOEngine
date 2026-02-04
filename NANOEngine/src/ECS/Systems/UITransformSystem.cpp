#include "UITransformSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/EntityMeta.hpp"
#include "../../EditorInterface/ECSExports.hpp"
#include <iostream>
#include <algorithm>

namespace NE::ECS::Systems {

    UITransformSystem::UITransformSystem(ComponentManager* cm) : m_cm(cm) {}

    void UITransformSystem::OnEntityAdded(Entity e) 
    {
        
    }

    void UITransformSystem::OnEntityRemoved(Entity e) 
    {
        
    }

    void UITransformSystem::Init() 
    {

    }

    void UITransformSystem::Update(double) 
    {
       
    }

    void UITransformSystem::Exit() 
    {

    }

} // namespace NE::ECS::Systems