#include "pch.h"
#include "UILayoutSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UILayoutGroup.hpp"
#include "../Components/UIGridLayoutGroup.hpp"
#include "../Components/UILayoutElement.hpp"
#include "../Components/Hierarchy.hpp"
#include "../Components/EntityMeta.hpp"
#include "../Components/UIAutoSize.hpp"
#include "../Components/UIText.hpp"
#include "../Components/UICanvas.hpp"

using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    UILayoutSystem::UILayoutSystem(ComponentManager* cm) : m_cm(cm) {}

    void UILayoutSystem::Init() {}
    void UILayoutSystem::Exit() {}
    void UILayoutSystem::OnEntityAdded(Entity) {}
    void UILayoutSystem::OnEntityRemoved(Entity) {}
    void UILayoutSystem::OnEntityActive(Entity /*entity*/) {}
    void UILayoutSystem::OnEntityInactive(Entity /*entity*/) {}

    void UILayoutSystem::ForceLayout(Entity entity)
    {
        if (!m_cm->HasComponent<UILayoutGroup>(entity) &&
            !m_cm->HasComponent<UIGridLayoutGroup>(entity)) return;

        if (m_cm->HasComponent<UILayoutGroup>(entity)) {
            auto& layout = m_cm->GetComponent<UILayoutGroup>(entity);
            ProcessLinearLayout(entity, layout.isHorizontal);
        } else {
            ProcessGridLayout(entity);
        }
    }

    static int GetHierarchyDepth(NE::ECS::ComponentManager* cm, NE::ECS::Entity e) {
        int depth = 0;
        Entity cur = e;
        while (cm->HasComponent<Hierarchy>(cur)) {
            Entity parent = cm->GetComponent<Hierarchy>(cur).parent;
            if (parent == NO_ENTITY) break;
            cur = parent;
            ++depth;
        }
        return depth;
    }

    void UILayoutSystem::Update(double)
    {
        // Phase 1: Auto-size containers before layout groups process them
        ProcessAutoSize();

        // Phase 2: Layout groups — sort by hierarchy depth so parents run before children
        // This ensures a parent layout group sets its children's sizes before nested
        // child layout groups use those sizes to position their own children.
        std::vector<Entity> layoutEntities;
        for (Entity e : GetEntities()) {
            if (m_cm->HasComponent<UILayoutGroup>(e) || m_cm->HasComponent<UIGridLayoutGroup>(e))
                layoutEntities.push_back(e);
        }
        std::sort(layoutEntities.begin(), layoutEntities.end(), [this](Entity a, Entity b) {
            return GetHierarchyDepth(m_cm, a) < GetHierarchyDepth(m_cm, b);
        });
        for (Entity e : layoutEntities) {
            if (m_cm->HasComponent<UILayoutGroup>(e)) {
                auto& layout = m_cm->GetComponent<UILayoutGroup>(e);
                ProcessLinearLayout(e, layout.isHorizontal);
            } else {
                ProcessGridLayout(e);
            }
        }
    }

    void UILayoutSystem::ProcessLinearLayout(Entity entity, bool isHorizontal)
    {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) return;
        if (!m_cm->HasComponent<Hierarchy>(entity)) return;
        if (!m_cm->HasComponent<UILayoutGroup>(entity)) return;

        auto& parentRect = m_cm->GetComponent<UIRectTransform>(entity);
        auto& hierarchy = m_cm->GetComponent<Hierarchy>(entity);
        auto& layout = m_cm->GetComponent<UILayoutGroup>(entity);

        float padLeft = layout.paddingLeft;
        float padRight = layout.paddingRight;
        float padTop = layout.paddingTop;
        float padBottom = layout.paddingBottom;
        float spacing = layout.spacing;
        int childAlignment = static_cast<int>(layout.childAlignment);
        bool controlChildWidth = layout.controlChildWidth;
        bool controlChildHeight = layout.controlChildHeight;
        bool forceExpandWidth = layout.childForceExpandWidth;
        bool forceExpandHeight = layout.childForceExpandHeight;
        bool reverseArrangement = layout.reverseArrangement;

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
            float minSize;
            float flexible;
            float crossPreferred;
        };
        std::vector<ChildInfo> childInfos(N);

        for (int i = 0; i < N; ++i) {
            auto& childRect = m_cm->GetComponent<UIRectTransform>(children[i]);
            float prefW = childRect.width;
            float prefH = childRect.height;
            float minW = 0.f;
            float minH = 0.f;
            float flexW = 0.f;
            float flexH = 0.f;

            if (m_cm->HasComponent<UILayoutElement>(children[i])) {
                auto& le = m_cm->GetComponent<UILayoutElement>(children[i]);
                if (le.preferredWidth >= 0.f) prefW = le.preferredWidth;
                if (le.preferredHeight >= 0.f) prefH = le.preferredHeight;
                if (le.minWidth >= 0.f) minW = le.minWidth;
                if (le.minHeight >= 0.f) minH = le.minHeight;
                if (le.flexibleWidth >= 0.f) flexW = le.flexibleWidth;
                if (le.flexibleHeight >= 0.f) flexH = le.flexibleHeight;
            }

            if (isHorizontal) {
                childInfos[i].preferred = prefW;
                childInfos[i].minSize = minW;
                childInfos[i].flexible = forceExpandWidth ? std::max(1.f, flexW) : flexW;
                childInfos[i].crossPreferred = prefH;
                totalPreferred += prefW;
                totalFlexible += childInfos[i].flexible;
            } else {
                childInfos[i].preferred = prefH;
                childInfos[i].minSize = minH;
                childInfos[i].flexible = forceExpandHeight ? std::max(1.f, flexH) : flexH;
                childInfos[i].crossPreferred = prefW;
                totalPreferred += prefH;
                totalFlexible += childInfos[i].flexible;
            }
        }

        float availableSpace = (isHorizontal ? containerWidth : containerHeight) - spacing * (N - 1);
        float extraSpace = availableSpace - totalPreferred;

        // Pass 2a: Compute initial final sizes for each child
        std::vector<float> finalSizes(N);
        for (int i = 0; i < N; ++i) {
            float mainSize = childInfos[i].preferred;
            if (extraSpace > 0.f && totalFlexible > 0.f) {
                mainSize += extraSpace * (childInfos[i].flexible / totalFlexible);
            } else if (extraSpace < 0.f) {
                if (totalFlexible > 0.f && childInfos[i].flexible > 0.f) {
                    float shrink = (-extraSpace) * (childInfos[i].flexible / totalFlexible);
                    mainSize -= shrink;
                } else if (totalFlexible <= 0.f) {
                    float shrinkPer = (-extraSpace) / static_cast<float>(N);
                    mainSize -= shrinkPer;
                }
            }
            finalSizes[i] = std::max(mainSize, childInfos[i].minSize);
        }

        // Pass 2b: Check for remaining overflow after min-size clamping and redistribute
        float actualTotal = spacing * (N - 1);
        for (int i = 0; i < N; ++i) actualTotal += finalSizes[i];
        float remaining = (isHorizontal ? containerWidth : containerHeight) - actualTotal;
        if (remaining < -0.01f) {
            // Still overflowing: distribute remaining deficit uniformly among children above minSize
            int shrinkable = 0;
            for (int i = 0; i < N; ++i)
                if (finalSizes[i] > childInfos[i].minSize + 0.01f) ++shrinkable;
            if (shrinkable > 0) {
                float shrinkPer = (-remaining) / static_cast<float>(shrinkable);
                for (int i = 0; i < N; ++i) {
                    if (finalSizes[i] > childInfos[i].minSize + 0.01f) {
                        finalSizes[i] = std::max(finalSizes[i] - shrinkPer, childInfos[i].minSize);
                    }
                }
            }
        }

        // Pass 3: Position and size each child
        float layoutPos = 0.f;

        for (int i = 0; i < N; ++i) {
            auto& childRect = m_cm->GetComponent<UIRectTransform>(children[i]);
            float mainSize = finalSizes[i];

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
            float crossOffset = 0.f;
            int alignRow = childAlignment / 3;
            int alignCol = childAlignment % 3;

            if (isHorizontal) {
                float actualCrossSize = childHeight;
                if (alignRow == 0) crossOffset = 0.f;
                else if (alignRow == 1) crossOffset = (crossContainer - actualCrossSize) * 0.5f;
                else crossOffset = crossContainer - actualCrossSize;
            } else {
                float actualCrossSize = childWidth;
                if (alignCol == 0) crossOffset = 0.f;
                else if (alignCol == 1) crossOffset = (crossContainer - actualCrossSize) * 0.5f;
                else crossOffset = crossContainer - actualCrossSize;
            }

            // Convert from top-left layout coords to center-anchor coords
            float halfParentW = parentRect.width * 0.5f;
            float halfParentH = parentRect.height * 0.5f;

            if (isHorizontal) {
                float layoutX = padLeft + layoutPos;
                float layoutY = padTop + crossOffset;
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

        if (grid.constraint == UIGridLayoutGroup::Constraint::FixedColumnCount) {
            columns = std::max(1, grid.constraintCount);
            rows = (static_cast<int>(children.size()) + columns - 1) / columns;
        } else if (grid.constraint == UIGridLayoutGroup::Constraint::FixedRowCount) {
            rows = std::max(1, grid.constraintCount);
            columns = (static_cast<int>(children.size()) + rows - 1) / rows;
        } else { // Flexible
            if (grid.startAxis == UIGridLayoutGroup::StartAxis::Horizontal) {
                columns = std::max(1, static_cast<int>((containerWidth + grid.spacingX) / (grid.cellWidth + grid.spacingX)));
                rows = (static_cast<int>(children.size()) + columns - 1) / columns;
            } else { // Vertical
                rows = std::max(1, static_cast<int>((containerHeight + grid.spacingY) / (grid.cellHeight + grid.spacingY)));
                columns = (static_cast<int>(children.size()) + rows - 1) / rows;
            }
        }

        // Compute effective cell dimensions (stretch mode fills available space evenly)
        float effectiveCellW = grid.cellWidth;
        float effectiveCellH = grid.cellHeight;
        if (grid.stretchCells && columns > 0 && rows > 0) {
            effectiveCellW = (containerWidth  - grid.spacingX * (columns - 1)) / static_cast<float>(columns);
            effectiveCellH = (containerHeight - grid.spacingY * (rows    - 1)) / static_cast<float>(rows);
            effectiveCellW = std::max(0.f, effectiveCellW);
            effectiveCellH = std::max(0.f, effectiveCellH);
        }

        float halfParentW = parentRect.width * 0.5f;
        float halfParentH = parentRect.height * 0.5f;

        for (int i = 0; i < static_cast<int>(children.size()); ++i) {
            auto& childRect = m_cm->GetComponent<UIRectTransform>(children[i]);

            int col, row;
            if (grid.startAxis == UIGridLayoutGroup::StartAxis::Horizontal) {
                col = i % columns;
                row = i / columns;
            } else { // Vertical
                row = i % rows;
                col = i / rows;
            }

            // Handle start corner
            if (grid.startCorner == UIGridLayoutGroup::StartCorner::UpperRight ||
                grid.startCorner == UIGridLayoutGroup::StartCorner::LowerRight) {
                col = (columns - 1) - col;
            }
            if (grid.startCorner == UIGridLayoutGroup::StartCorner::LowerLeft ||
                grid.startCorner == UIGridLayoutGroup::StartCorner::LowerRight) {
                row = (rows - 1) - row;
            }

            float cellX = grid.paddingLeft + col * (effectiveCellW + grid.spacingX);
            float cellY = grid.paddingTop  + row * (effectiveCellH + grid.spacingY);

            childRect.width  = effectiveCellW;
            childRect.height = effectiveCellH;

            // Convert from top-left layout coords to center-anchor coords
            childRect.x = cellX - halfParentW + effectiveCellW * childRect.pivotX;
            childRect.y = cellY - halfParentH + effectiveCellH * childRect.pivotY;

            childRect.worldMatrixDirty = true;
            childRect.worldRectCached = false;
        }
    }

    void UILayoutSystem::ProcessAutoSize()
    {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIAutoSize>(e)) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;

            auto& fitter = m_cm->GetComponent<UIAutoSize>(e);
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);
            bool changed = false;

            // === Content Size Fitter ===
            if (fitter.horizontalFit != UIAutoSize::FitMode::Unconstrained ||
                fitter.verticalFit   != UIAutoSize::FitMode::Unconstrained) {
                float preferredW = rect.width;
                float preferredH = rect.height;

                // Priority 1: If entity has UIText, use text bounds as preferred size
                if (m_cm->HasComponent<UIText>(e)) {
                    auto& text = m_cm->GetComponent<UIText>(e);
                    if (text.cachedSize.x > 0.f || text.cachedSize.y > 0.f) {
                        preferredW = text.cachedSize.x;
                        preferredH = text.cachedSize.y;
                    }
                }
                // Priority 2: If entity has children, use content bounds
                else if (m_cm->HasComponent<Hierarchy>(e) && m_layoutEngine) {
                    auto& hierarchy = m_cm->GetComponent<Hierarchy>(e);
                    if (!hierarchy.children.empty()) {
                        auto bounds = m_layoutEngine->CalculateContentBounds(e);
                        if (bounds.width > 0.f) preferredW = bounds.width;
                        if (bounds.height > 0.f) preferredH = bounds.height;
                    }
                }

                // horizontalFit: PreferredSize sets width to measured preferred
                if (fitter.horizontalFit == UIAutoSize::FitMode::PreferredSize) {
                    if (std::abs(rect.width - preferredW) > 0.01f) {
                        rect.width = preferredW;
                        changed = true;
                    }
                }

                // verticalFit: PreferredSize sets height to measured preferred
                if (fitter.verticalFit == UIAutoSize::FitMode::PreferredSize) {
                    if (std::abs(rect.height - preferredH) > 0.01f) {
                        rect.height = preferredH;
                        changed = true;
                    }
                }
            }

            // === Aspect Ratio Fitter ===
            if (fitter.aspectMode != UIAutoSize::AspectMode::None && fitter.aspectRatio > 0.f) {
                switch (fitter.aspectMode) {
                case UIAutoSize::AspectMode::WidthControlsHeight: {
                    float newH = rect.width / fitter.aspectRatio;
                    if (std::abs(rect.height - newH) > 0.01f) {
                        rect.height = newH;
                        changed = true;
                    }
                    break;
                }
                case UIAutoSize::AspectMode::HeightControlsWidth: {
                    float newW = rect.height * fitter.aspectRatio;
                    if (std::abs(rect.width - newW) > 0.01f) {
                        rect.width = newW;
                        changed = true;
                    }
                    break;
                }
                case UIAutoSize::AspectMode::FitInParent:
                case UIAutoSize::AspectMode::EnvelopeParent:
                {
                    float parentW = rect.width;
                    float parentH = rect.height;

                    // Get parent dimensions
                    if (m_cm->HasComponent<Hierarchy>(e)) {
                        Entity parent = m_cm->GetComponent<Hierarchy>(e).parent;
                        if (parent != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(parent)) {
                            auto& pRect = m_cm->GetComponent<UIRectTransform>(parent);
                            parentW = pRect.width;
                            parentH = pRect.height;
                        }
                    }

                    float parentAspect = (parentH > 0.f) ? parentW / parentH : 1.f;
                    float newW, newH;

                    if (fitter.aspectMode == UIAutoSize::AspectMode::FitInParent) {
                        // FitInParent: Fit inside parent, maintaining ratio
                        if (fitter.aspectRatio > parentAspect) {
                            newW = parentW;
                            newH = parentW / fitter.aspectRatio;
                        } else {
                            newH = parentH;
                            newW = parentH * fitter.aspectRatio;
                        }
                    } else {
                        // EnvelopeParent: Fill parent, may overflow
                        if (fitter.aspectRatio > parentAspect) {
                            newH = parentH;
                            newW = parentH * fitter.aspectRatio;
                        } else {
                            newW = parentW;
                            newH = parentW / fitter.aspectRatio;
                        }
                    }

                    if (std::abs(rect.width - newW) > 0.01f || std::abs(rect.height - newH) > 0.01f) {
                        rect.width = newW;
                        rect.height = newH;
                        changed = true;
                    }
                    break;
                }
                }
            }

            if (changed) {
                rect.worldMatrixDirty = true;
                rect.worldRectCached = false;
            }
        }
    }

} // namespace NE::ECS::Systems