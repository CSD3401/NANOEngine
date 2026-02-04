#include "UITransformSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/EntityMeta.hpp"
#include "../../EditorInterface/ECSExports.hpp"
#include <iostream>
#include <algorithm>

namespace NE::ECS::Systems {

    UITransformSystem::UITransformSystem(ComponentManager* cm) : m_cm(cm) {}

    void UITransformSystem::OnEntityAdded(Entity e) {
        if (!m_cm->HasComponent<Component::UIRectTransform>(e)) return;

        auto& rect = m_cm->GetComponent<Component::UIRectTransform>(e);

        // register luid for entity mapping
        if (rect.luid != 0)
        {
            m_luidToEntity[rect.luid] = e;
        }

        // queue parent resolution if needed
        if (rect.parentLuid != 0) 
        {
            m_pendingParents.push_back(PendingParent{ e, rect.parentLuid });
        }
    }

    void UITransformSystem::OnEntityRemoved(Entity e) {
        if (!m_cm->HasComponent<Component::UIRectTransform>(e)) return;

        auto& rect = m_cm->GetComponent<Component::UIRectTransform>(e);

        // remove from luid map
        if (rect.luid != 0)
        {
            m_luidToEntity.erase(rect.luid);
        }

        // remove from pending parents list
        m_pendingParents.erase(
            std::remove_if(m_pendingParents.begin(), m_pendingParents.end(),
                [e](const PendingParent& pp) { return pp.child == e; }),
            m_pendingParents.end()
        );

        // Just orphan direct children (they may not be destroyed)
        const auto& entities = GetEntities();
        for (Entity child : entities)
        {
            if (child == e) continue;
            if (!m_cm->HasComponent<Component::UIRectTransform>(child)) continue;
            auto& childRect = m_cm->GetComponent<Component::UIRectTransform>(child);
            if (childRect.parent == e)
            {
                childRect.parent = NO_ENTITY;
                childRect.parentLuid = 0;
            }
        }
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