#include "UITransformSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/Hierarchy.hpp"
#include "../Components/EntityMeta.hpp"
#include "../../EditorInterface/ECSExports.hpp"
#include <iostream>
#include <algorithm>

namespace NE::ECS::Systems {

    // NOTE: This system is now largely obsolete since Hierarchy component handles parent-child relationships.
    // All hierarchy management (parent, luid, parentLuid) is now handled by HierarchySystem.
    // Keeping stub implementation for backward compatibility until fully deprecated.

    UITransformSystem::UITransformSystem(ComponentManager* cm) : m_cm(cm) {}

    void UITransformSystem::OnEntityAdded(Entity e) {
        // DEPRECATED: Hierarchy component now handles parent-child relationships
        (void)e;
    }

    void UITransformSystem::OnEntityRemoved(Entity e) {
        // DEPRECATED: Hierarchy component now handles parent-child relationships
        (void)e;
    }

    void UITransformSystem::Init() {
        // DEPRECATED: HierarchySystem now handles LUID mapping and parent relationships
        m_luidToEntity.clear();
        m_pendingParents.clear();
    }

    void UITransformSystem::ResolvePendingParents() {
        // DEPRECATED: HierarchySystem now handles this
    }

    void UITransformSystem::SetParent(Entity child, Entity newParent) {
        // DEPRECATED: Use HierarchySystem::SetParent instead
        (void)child;
        (void)newParent;
    }

    Entity UITransformSystem::GetEntityFromLUID(uint64_t luid) const {
        // DEPRECATED: Use HierarchySystem for LUID lookup
        (void)luid;
        return NO_ENTITY;
    }

    void UITransformSystem::Update(double deltaTime) {
        // DEPRECATED: No UI-specific transform updates needed
        // UIRenderSystem handles transform calculations during rendering
        (void)deltaTime;
    }

    void UITransformSystem::Exit() {
        // Nothing to clean up
    }

} // namespace NE::ECS::Systems
