#include "UILayoutSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIHorizontalLayoutGroup.hpp"
#include "../Components/UIVerticalLayoutGroup.hpp"
#include "../Components/UIGridLayoutGroup.hpp"
#include "../Components/UILayoutElement.hpp"
#include "../Components/Hierarchy.hpp"
#include "../Components/EntityMeta.hpp"
#include <algorithm>
#include <cmath>

using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    UILayoutSystem::UILayoutSystem(ComponentManager* cm) : m_cm(cm) {}

    void UILayoutSystem::Init() {}
    void UILayoutSystem::Exit() {}
    void UILayoutSystem::OnEntityAdded(Entity) {}
    void UILayoutSystem::OnEntityRemoved(Entity) {}

    void UILayoutSystem::Update(double)
    {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (m_cm->HasComponent<UIHorizontalLayoutGroup>(e)) {
                ProcessLinearLayout(e, true);
            }
            else if (m_cm->HasComponent<UIVerticalLayoutGroup>(e)) {
                ProcessLinearLayout(e, false);
            }
            else if (m_cm->HasComponent<UIGridLayoutGroup>(e)) {
                ProcessGridLayout(e);
            }
        }
    }

    void UILayoutSystem::ProcessLinearLayout(Entity entity, bool isHorizontal)
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) return;
        if (!m_cm->HasComponent<Hierarchy>(entity)) return;

        auto& parentRect = m_cm->GetComponent<UIRectTransform>(entity);
        auto& hierarchy = m_cm->GetComponent<Hierarchy>(entity);

        // Get layout group settings
        float padLeft = 0.f, padRight = 0.f, padTop = 0.f, padBottom = 0.f;
        float spacing = 0.f;
        int childAlignment = 0;
        bool controlChildWidth = true, controlChildHeight = true;
        bool forceExpandWidth = true, forceExpandHeight = true;
        bool reverseArrangement = false;

        if (isHorizontal && m_cm->HasComponent<UIHorizontalLayoutGroup>(entity)) {
            auto& layout = m_cm->GetComponent<UIHorizontalLayoutGroup>(entity);
            padLeft = layout.paddingLeft;
            padRight = layout.paddingRight;
            padTop = layout.paddingTop;
            padBottom = layout.paddingBottom;
            spacing = layout.spacing;
            childAlignment = layout.childAlignment;
            controlChildWidth = layout.controlChildWidth;
            controlChildHeight = layout.controlChildHeight;
            forceExpandWidth = layout.childForceExpandWidth;
            forceExpandHeight = layout.childForceExpandHeight;
            reverseArrangement = layout.reverseArrangement;
        }
        else if (!isHorizontal && m_cm->HasComponent<UIVerticalLayoutGroup>(entity)) {
            auto& layout = m_cm->GetComponent<UIVerticalLayoutGroup>(entity);
            padLeft = layout.paddingLeft;
            padRight = layout.paddingRight;
            padTop = layout.paddingTop;
            padBottom = layout.paddingBottom;
            spacing = layout.spacing;
            childAlignment = layout.childAlignment;
            controlChildWidth = layout.controlChildWidth;
            controlChildHeight = layout.controlChildHeight;
            forceExpandWidth = layout.childForceExpandWidth;
            forceExpandHeight = layout.childForceExpandHeight;
            reverseArrangement = layout.reverseArrangement;
        }
        else {
            return;
        }

        // Collect active children (skip ignoreLayout and inactive)
        std::vector<Entity> children;
        for (uint32_t child : hierarchy.children) {
            if (!m_cm->HasComponent<UIRectTransform>(child)) continue;

            // Check if inactive
            if (m_cm->HasComponent<EntityMeta>(child)) {
                if (!m_cm->GetComponent<EntityMeta>(child).isActive) continue;
            }

            // Check if ignoreLayout
            if (m_cm->HasComponent<UILayoutElement>(child)) {
                if (m_cm->GetComponent<UILayoutElement>(child).ignoreLayout) continue;
            }

            children.push_back(child);
        }

        if (children.empty()) return;

        if (reverseArrangement) {
            std::reverse(children.begin(), children.end());
        }

        float containerWidth = parentRect.width - padLeft - padRight;
        float containerHeight = parentRect.height - padTop - padBottom;
        int N = static_cast<int>(children.size());

        // Pass 1: Sum preferred sizes and flexible weights
        float totalPreferred = 0.f;
        float totalFlexible = 0.f;

        struct ChildInfo {
            float preferred;
            float flexible;
            float crossPreferred;
        };
        std::vector<ChildInfo> childInfos(N);

        for (int i = 0; i < N; ++i) {
            auto& childRect = m_cm->GetComponent<UIRectTransform>(children[i]);
            float prefW = childRect.width;
            float prefH = childRect.height;
            float flexW = 0.f;
            float flexH = 0.f;

            if (m_cm->HasComponent<UILayoutElement>(children[i])) {
                auto& le = m_cm->GetComponent<UILayoutElement>(children[i]);
                if (le.preferredWidth >= 0.f) prefW = le.preferredWidth;
                if (le.preferredHeight >= 0.f) prefH = le.preferredHeight;
                if (le.flexibleWidth >= 0.f) flexW = le.flexibleWidth;
                if (le.flexibleHeight >= 0.f) flexH = le.flexibleHeight;
            }

            if (isHorizontal) {
                childInfos[i].preferred = prefW;
                childInfos[i].flexible = forceExpandWidth ? std::max(1.f, flexW) : flexW;
                childInfos[i].crossPreferred = prefH;
                totalPreferred += prefW;
                totalFlexible += childInfos[i].flexible;
            } else {
                childInfos[i].preferred = prefH;
                childInfos[i].flexible = forceExpandHeight ? std::max(1.f, flexH) : flexH;
                childInfos[i].crossPreferred = prefW;
                totalPreferred += prefH;
                totalFlexible += childInfos[i].flexible;
            }
        }

        float availableSpace = (isHorizontal ? containerWidth : containerHeight) - spacing * (N - 1);
        float extraSpace = availableSpace - totalPreferred;
        if (extraSpace < 0.f) extraSpace = 0.f;

        // Pass 2: Position and size each child
        float layoutPos = 0.f; // Position along main axis in layout-local coords (from top-left)

        for (int i = 0; i < N; ++i) {
            auto& childRect = m_cm->GetComponent<UIRectTransform>(children[i]);

            // Calculate size along main axis
            float mainSize = childInfos[i].preferred;
            if (totalFlexible > 0.f && extraSpace > 0.f) {
                mainSize += extraSpace * (childInfos[i].flexible / totalFlexible);
            }

            // Calculate size along cross axis
            float crossSize = childInfos[i].crossPreferred;
            bool forceExpandCross = isHorizontal ? forceExpandHeight : forceExpandWidth;
            bool controlCross = isHorizontal ? controlChildHeight : controlChildWidth;
            float crossContainer = isHorizontal ? containerHeight : containerWidth;
            if (forceExpandCross && controlCross) {
                crossSize = crossContainer;
            }

            // Set child dimensions
            float childWidth, childHeight;
            if (isHorizontal) {
                childWidth = controlChildWidth ? mainSize : childRect.width;
                childHeight = controlChildHeight ? crossSize : childRect.height;
            } else {
                childWidth = controlChildWidth ? crossSize : childRect.width;
                childHeight = controlChildHeight ? mainSize : childRect.height;
            }

            if (controlChildWidth) childRect.width = childWidth;
            if (controlChildHeight) childRect.height = childHeight;

            // Calculate cross-axis offset based on childAlignment
            // Alignment rows: 0-2 = Upper, 3-5 = Middle, 6-8 = Lower
            // Alignment cols: 0,3,6 = Left, 1,4,7 = Center, 2,5,8 = Right
            float crossOffset = 0.f;
            int alignRow = childAlignment / 3; // 0=Upper, 1=Middle, 2=Lower
            int alignCol = childAlignment % 3; // 0=Left, 1=Center, 2=Right

            if (isHorizontal) {
                // Cross axis is vertical
                float actualCrossSize = childHeight;
                if (alignRow == 0) crossOffset = 0.f; // Upper
                else if (alignRow == 1) crossOffset = (crossContainer - actualCrossSize) * 0.5f; // Middle
                else crossOffset = crossContainer - actualCrossSize; // Lower
            } else {
                // Cross axis is horizontal
                float actualCrossSize = childWidth;
                if (alignCol == 0) crossOffset = 0.f; // Left
                else if (alignCol == 1) crossOffset = (crossContainer - actualCrossSize) * 0.5f; // Center
                else crossOffset = crossContainer - actualCrossSize; // Right
            }

            // Convert from top-left layout coords to center-anchor coords
            // The engine uses center-anchor (0,0 = center of parent)
            float halfParentW = parentRect.width * 0.5f;
            float halfParentH = parentRect.height * 0.5f;

            if (isHorizontal) {
                float layoutX = padLeft + layoutPos;
                float layoutY = padTop + crossOffset;

                // Convert: x = layoutX - halfParentW + childWidth * pivotX
                childRect.x = layoutX - halfParentW + childWidth * childRect.pivotX;
                childRect.y = layoutY - halfParentH + childHeight * childRect.pivotY;
            } else {
                float layoutX = padLeft + crossOffset;
                float layoutY = padTop + layoutPos;

                childRect.x = layoutX - halfParentW + childWidth * childRect.pivotX;
                childRect.y = layoutY - halfParentH + childHeight * childRect.pivotY;
            }

            float actualMainSize = isHorizontal ? childWidth : childHeight;
            layoutPos += actualMainSize + spacing;

            // Mark world matrix as dirty since we changed the rect
            childRect.worldMatrixDirty = true;
            childRect.worldRectCached = false;
        }
    }

    void UILayoutSystem::ProcessGridLayout(Entity entity)
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) return;
        if (!m_cm->HasComponent<Hierarchy>(entity)) return;
        if (!m_cm->HasComponent<UIGridLayoutGroup>(entity)) return;

        auto& parentRect = m_cm->GetComponent<UIRectTransform>(entity);
        auto& hierarchy = m_cm->GetComponent<Hierarchy>(entity);
        auto& grid = m_cm->GetComponent<UIGridLayoutGroup>(entity);

        // Collect active children
        std::vector<Entity> children;
        for (uint32_t child : hierarchy.children) {
            if (!m_cm->HasComponent<UIRectTransform>(child)) continue;

            if (m_cm->HasComponent<EntityMeta>(child)) {
                if (!m_cm->GetComponent<EntityMeta>(child).isActive) continue;
            }

            if (m_cm->HasComponent<UILayoutElement>(child)) {
                if (m_cm->GetComponent<UILayoutElement>(child).ignoreLayout) continue;
            }

            children.push_back(child);
        }

        if (children.empty()) return;

        float containerWidth = parentRect.width - grid.paddingLeft - grid.paddingRight;
        float containerHeight = parentRect.height - grid.paddingTop - grid.paddingBottom;

        // Calculate column and row count
        int columns = 1;
        int rows = 1;

        if (grid.constraint == 1) { // FixedColumnCount
            columns = std::max(1, grid.constraintCount);
            rows = (static_cast<int>(children.size()) + columns - 1) / columns;
        } else if (grid.constraint == 2) { // FixedRowCount
            rows = std::max(1, grid.constraintCount);
            columns = (static_cast<int>(children.size()) + rows - 1) / rows;
        } else { // Flexible
            if (grid.startAxis == 0) { // Horizontal
                columns = std::max(1, static_cast<int>((containerWidth + grid.spacingX) / (grid.cellWidth + grid.spacingX)));
                rows = (static_cast<int>(children.size()) + columns - 1) / columns;
            } else { // Vertical
                rows = std::max(1, static_cast<int>((containerHeight + grid.spacingY) / (grid.cellHeight + grid.spacingY)));
                columns = (static_cast<int>(children.size()) + rows - 1) / rows;
            }
        }

        float halfParentW = parentRect.width * 0.5f;
        float halfParentH = parentRect.height * 0.5f;

        for (int i = 0; i < static_cast<int>(children.size()); ++i) {
            auto& childRect = m_cm->GetComponent<UIRectTransform>(children[i]);

            int col, row;
            if (grid.startAxis == 0) { // Horizontal
                col = i % columns;
                row = i / columns;
            } else { // Vertical
                row = i % rows;
                col = i / rows;
            }

            // Handle start corner
            if (grid.startCorner == 1 || grid.startCorner == 3) { // Right corners
                col = (columns - 1) - col;
            }
            if (grid.startCorner == 2 || grid.startCorner == 3) { // Lower corners
                row = (rows - 1) - row;
            }

            float cellX = grid.paddingLeft + col * (grid.cellWidth + grid.spacingX);
            float cellY = grid.paddingTop + row * (grid.cellHeight + grid.spacingY);

            childRect.width = grid.cellWidth;
            childRect.height = grid.cellHeight;

            // Convert from top-left layout coords to center-anchor coords
            childRect.x = cellX - halfParentW + grid.cellWidth * childRect.pivotX;
            childRect.y = cellY - halfParentH + grid.cellHeight * childRect.pivotY;

            childRect.worldMatrixDirty = true;
            childRect.worldRectCached = false;
        }
    }

} // namespace NE::ECS::Systems
