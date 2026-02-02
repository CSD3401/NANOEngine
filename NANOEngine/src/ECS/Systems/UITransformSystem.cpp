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

        // REMOVED THE CANVAS DESTRUCTION LOGIC - Let the editor handle hierarchy
        // The editor's DeleteEntityCommand already handles destroying descendants

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

    void UITransformSystem::Init() {
        ResolvePendingParents();
    }

    void UITransformSystem::ResolvePendingParents() {
        std::vector<PendingParent> stillPending;
        stillPending.reserve(m_pendingParents.size());

        for (const PendingParent& pp : m_pendingParents) {
            if (!m_cm->HasComponent<Component::UIRectTransform>(pp.child))
                continue;

            auto& childRect = m_cm->GetComponent<Component::UIRectTransform>(pp.child);

            // look up parent entity by luid
            auto it = m_luidToEntity.find(pp.parentLuid);
            if (it != m_luidToEntity.end()) 
            {
                Entity parentEnt = it->second;
                childRect.parent = parentEnt;
            }
            else 
            {
                stillPending.push_back(pp);
            }
        }

        m_pendingParents.swap(stillPending);
    }

    void UITransformSystem::SetParent(Entity child, Entity newParent) {
        if (!m_cm->HasComponent<Component::UIRectTransform>(child)) return;

        auto& childRect = m_cm->GetComponent<Component::UIRectTransform>(child);
        childRect.parent = newParent;

        if (newParent != NO_ENTITY && m_cm->HasComponent<Component::UIRectTransform>(newParent)) 
        {
            auto& parentRect = m_cm->GetComponent<Component::UIRectTransform>(newParent);
            childRect.parentLuid = parentRect.luid;
        }
        else 
        {
            childRect.parentLuid = 0;
        }
    }

    Entity UITransformSystem::GetEntityFromLUID(uint64_t luid) const {
        auto it = m_luidToEntity.find(luid);
        if (it != m_luidToEntity.end()) 
        {
            return it->second;
        }
        return NO_ENTITY;
    }

    void UITransformSystem::Update(double) {
        // Resolve any pending parent relationships (needed after scene load)
        if (!m_pendingParents.empty()) {
            ResolvePendingParents();
        }

        // Future: calculate world-space transforms for UI elements
        // Similar to how TransformSystem builds world matrices
        // shift matrix calculations from uigizmo handling here
        // and also matrix calculations done in scene panel
        // TODO
    }

    void UITransformSystem::Exit() {}

} // namespace NE::ECS::Systems