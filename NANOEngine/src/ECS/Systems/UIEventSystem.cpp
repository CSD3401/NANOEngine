#include "pch.h"
#include "UIEventSystem.hpp"
#include "../../Input/InputManager.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/EditorCamera.hpp"
#include "../Components/Transform.hpp"
#include "../../Math/Vec4.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include "../Components/UIRectTransform.hpp"
// UIRectMask2D folded into UIRectTransform (enableMask + maskPadding fields)
#include "../Components/UIScrollRect.hpp"
#include "../Components/Hierarchy.hpp"
#include "../../Events/EventBus.hpp"
#include "../../Events/UIEvents.hpp"
#include "UITransformUtilities.hpp"
#include "../Components/UIInputField.hpp"
#include "../Components/UIDropdown.hpp"
#include "../Components/UIText.hpp"
#include "Engine.hpp"
#include "glfw/glfw3.h"

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    static constexpr float DEFAULT_ANCHOR_X = 0.5f;
    static constexpr float DEFAULT_ANCHOR_Y = 0.5f;
    static constexpr float PI = 3.14159265358979f;
    static constexpr float DEFAULT_WORLD_SPACE_SCALE = 0.1f;

    // Static viewport transform members
    bool UIEventSystem::s_useViewportTransform = false;
    float UIEventSystem::s_viewportOffsetX = 0.0f;
    float UIEventSystem::s_viewportOffsetY = 0.0f;
    float UIEventSystem::s_viewportWidth = 1920.0f;
    float UIEventSystem::s_viewportHeight = 1080.0f;
    float UIEventSystem::s_uiWidth = 1920.0f;
    float UIEventSystem::s_uiHeight = 1080.0f;

    UIEventSystem::UIEventSystem(ComponentManager* cm, EntityManager* em) : m_cm(cm), m_em(em) {}

    void UIEventSystem::Init() {}
    Entity UIEventSystem::FindOwningCanvas(Entity entity) const
    {
        if (m_layoutEngine) {
            return m_layoutEngine->FindOwningCanvas(entity);
        }

        if (!m_cm->HasComponent<UIRectTransform>(entity)) return NO_ENTITY;

        Entity cur = entity;
        while (cur != NO_ENTITY)
        {
            if (m_cm->HasComponent<UICanvas>(cur)) return cur;
            if (!m_cm->HasComponent<Hierarchy>(cur)) break;
            cur = m_cm->GetComponent<Hierarchy>(cur).parent;
        }
        return NO_ENTITY;
    }

    bool UIEventSystem::IsActiveForUI(Entity entity, Entity canvasEntity) const
    {
        return UIUtil::IsActiveForUI(m_cm, m_em, entity, canvasEntity);
    }

    void UIEventSystem::OnEntityAdded(Entity e) {}

    void UIEventSystem::OnEntityRemoved(Entity e) {
        if (e == m_hoveredEntity) {
            m_hoveredEntity = NO_ENTITY;
        }
        if (e == m_pressedEntity) {
            m_pressedEntity = NO_ENTITY;
        }
        if (e == m_focusedEntity) {
            ClearFocus();
        }
        if (e == m_expandedDropdown) {
            CollapseDropdown();
        }
    }

    void UIEventSystem::OnEntityActive(Entity /*entity*/) {}
    void UIEventSystem::OnEntityInactive(Entity e) {
        if (e == m_focusedEntity) {
            ClearFocus();
        }
    }

    void UIEventSystem::Exit() {}

    void UIEventSystem::Update(double deltaTime) {
        // Clear wasClicked and valueChanged flags at the start of each frame
        const auto& entities = GetEntities();
        for (Entity e : entities) {
            if (m_cm->HasComponent<UIButton>(e)) {
                auto& button = m_cm->GetComponent<UIButton>(e);
                button.wasClicked = false;
            }
            if (m_cm->HasComponent<UISlider>(e)) {
                auto& slider = m_cm->GetComponent<UISlider>(e);
                slider.valueChanged = false;
            }
            if (m_cm->HasComponent<UIToggle>(e)) {
                auto& toggle = m_cm->GetComponent<UIToggle>(e);
                toggle.valueChanged = false;
                toggle.wasClicked = false;
            }
            // Ensure dropdown panels start hidden and caption text stays in sync
            if (m_cm->HasComponent<UIDropdown>(e)) {
                auto& dropdown = m_cm->GetComponent<UIDropdown>(e);
                if (!dropdown.isExpanded && dropdown.optionsPanelEntity != UINT32_MAX) {
                    //auto& panelMeta = m_cm->GetComponent<EntityMeta>(dropdown.optionsPanelEntity);
                    //if (panelMeta.isActive && m_expandedDropdown != e) {
                    //    panelMeta.isActive = false;
                    //}

                    // might need to rewire to use ECSCoordinator->ToggleActive to properly do OnEnable callbacks
                    if (m_em->GetActive(dropdown.optionsPanelEntity) && m_expandedDropdown != e)
                        m_em->ToggleActive(dropdown.optionsPanelEntity, false);
                }
                // Always keep caption text up to date
                SyncDropdownCaptionText(e);
            }
        }

        // Get mouse position and transform to UI coords if viewport is set
        auto [mouseX, mouseY] = NE::InputManager::MousePos();
        TransformMouseToUICoords(mouseX, mouseY);

        bool mouseDown = NE::InputManager::IsMouseDown(0);
        bool mousePressed = NE::InputManager::WasMousePressed(0);
        bool mouseReleased = NE::InputManager::WasMouseReleased(0);

        // Collect all interactable UI elements (screen-space)
        std::vector<UIElementInfo> elements = CollectInteractableElements();

        // Sort by Z-order (higher Z = on top = checked first for hits)
        std::sort(elements.begin(), elements.end(),
            [](const UIElementInfo& a, const UIElementInfo& b) {
                return a.zOrder > b.zOrder;
            });

        // Find topmost screen-space element under mouse (with mask awareness)
        Entity hoveredEntity = NO_ENTITY;
        for (const auto& elem : elements) {
            if (PointInRect(static_cast<float>(mouseX), static_cast<float>(mouseY), elem)) {
                // Check if point is within all ancestor RectMask2D bounds
                if (IsPointInMaskBounds(elem.entity, static_cast<float>(mouseX), static_cast<float>(mouseY))) {
                    hoveredEntity = elem.entity;
                    break;
                }
            }
        }

        // If no screen-space hit, check world-space elements
        if (hoveredEntity == NO_ENTITY) {
            hoveredEntity = FindWorldSpaceHit(static_cast<float>(mouseX), static_cast<float>(mouseY));
        }

        // Handle press/release state
        if (mousePressed && hoveredEntity != NO_ENTITY) {
            m_pressedEntity = hoveredEntity;
        }

        // Update drag events BEFORE clearing m_pressedEntity on release, so the release
        // frame still has a valid entity reference for the final drag delta / drag-end event.
        UpdateDragEvents(static_cast<float>(mouseX), static_cast<float>(mouseY), mouseDown, mousePressed, mouseReleased);

        if (mouseReleased) {
            // Check for click (release over same entity that was pressed)
            if (m_pressedEntity != NO_ENTITY && m_pressedEntity == hoveredEntity) {
                if (m_cm->HasComponent<UIButton>(m_pressedEntity)) {
                    auto& button = m_cm->GetComponent<UIButton>(m_pressedEntity);
                    if (button.interactable) {
                        button.wasClicked = true;
                        NANOEngine::Events::EventBus::Get().Dispatch(
                            NANOEngine::Events::EventDomain::Engine,
                            NANOEngine::Events::UIButtonClickEvent{ m_pressedEntity, button.onClickEventId }
                        );
                    }
                }
                // Handle toggle click
                if (m_cm->HasComponent<UIToggle>(m_pressedEntity)) {
                    auto& toggle = m_cm->GetComponent<UIToggle>(m_pressedEntity);
                    if (toggle.interactable) {
                        toggle.wasClicked = true;
                        toggle.Toggle();
                        NANOEngine::Events::EventBus::Get().Dispatch(
                            NANOEngine::Events::EventDomain::Engine,
                            NANOEngine::Events::UIToggleChangedEvent{ m_pressedEntity, toggle.isOn }
                        );
                    }
                }
                // Also check if this is a toggle background
                if (m_cm->HasComponent<Hierarchy>(m_pressedEntity)) {
                    Entity parent = m_cm->GetComponent<Hierarchy>(m_pressedEntity).parent;
                    if (parent != NO_ENTITY && m_cm->HasComponent<UIToggle>(parent)) {
                        auto& toggle = m_cm->GetComponent<UIToggle>(parent);
                        if (toggle.background == m_pressedEntity && toggle.interactable) {
                            toggle.wasClicked = true;
                            toggle.Toggle();
                            NANOEngine::Events::EventBus::Get().Dispatch(
                                NANOEngine::Events::EventDomain::Engine,
                                NANOEngine::Events::UIToggleChangedEvent{ parent, toggle.isOn }
                            );
                        }
                    }
                }
            }
            m_pressedEntity = NO_ENTITY;
        }

        // Dispatch pointer enter/exit events on hover change
        if (hoveredEntity != m_hoveredEntity) {
            if (m_hoveredEntity != NO_ENTITY) {
                NANOEngine::Events::EventBus::Get().Dispatch(
                    NANOEngine::Events::EventDomain::Engine,
                    NANOEngine::Events::UIPointerExitEvent{ m_hoveredEntity }
                );
            }
            if (hoveredEntity != NO_ENTITY) {
                NANOEngine::Events::EventBus::Get().Dispatch(
                    NANOEngine::Events::EventDomain::Engine,
                    NANOEngine::Events::UIPointerEnterEvent{ hoveredEntity }
                );
            }
        }

        m_hoveredEntity = hoveredEntity;

        // Handle focus changes (click on input field = focus, click elsewhere = blur)
        HandleFocusChange(hoveredEntity, mousePressed);

        // Update button states
        UpdateButtonStates();

        // Update slider states (drag handling)
        UpdateSliderStates(static_cast<float>(mouseX), static_cast<float>(mouseY), mouseDown, mousePressed, mouseReleased);

        // Update toggle states
        UpdateToggleStates();

        // Update scroll rects (drag, inertia, elastic bounce)
        UpdateScrollRects(static_cast<float>(mouseX), static_cast<float>(mouseY), mouseDown, mousePressed, mouseReleased, deltaTime);

        // Update dropdowns (expand/collapse, option selection, caption sync)
        UpdateDropdowns(static_cast<float>(mouseX), static_cast<float>(mouseY), mousePressed, mouseReleased);

        // Update input fields (keyboard input, cursor blink, color)
        UpdateInputFields(deltaTime);
    }

    std::vector<UIEventSystem::UIElementInfo> UIEventSystem::CollectInteractableElements() {
        std::vector<UIElementInfo> elements;
        const auto& allEntities = GetEntities();

        // First, find all canvases
        std::vector<std::pair<Entity, UICanvas*>> canvases;
        for (Entity e : allEntities) {
            if (m_cm->HasComponent<UICanvas>(e)) {
                auto& canvas = m_cm->GetComponent<UICanvas>(e);
                // Only process screen space canvases for hit testing
                bool metaActive = m_em->GetActive(e);

                if (canvas.isActive && metaActive &&
                    (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
                        canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA)) {
                    canvases.push_back({ e, &canvas });
                }

            }
        }

        // For each canvas, find interactable entities (buttons, sliders, toggles)
        for (const auto& [canvasEntity, canvasPtr] : canvases) {
            if (!canvasPtr) continue;
            for (Entity e : allEntities) {
                if (e == canvasEntity) continue;
                if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
                if (!IsActiveForUI(e, canvasEntity)) continue;
                // Check if entity is interactable (button, slider, or toggle background)
                bool isInteractable = false;

                if (m_cm->HasComponent<UIButton>(e)) {
                    auto& button = m_cm->GetComponent<UIButton>(e);
                    if (button.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UISlider>(e)) {
                    auto& slider = m_cm->GetComponent<UISlider>(e);
                    if (slider.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UIToggle>(e)) {
                    auto& toggle = m_cm->GetComponent<UIToggle>(e);
                    if (toggle.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UIInputField>(e)) {
                    auto& inputField = m_cm->GetComponent<UIInputField>(e);
                    if (inputField.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UIDropdown>(e)) {
                    auto& dropdown = m_cm->GetComponent<UIDropdown>(e);
                    if (dropdown.interactable) isInteractable = true;
                }
                // Check if this is a toggle background (has image with raycastTarget)
                if (m_cm->HasComponent<UIImage>(e)) {
                    auto& image = m_cm->GetComponent<UIImage>(e);
                    if (image.raycastTarget) isInteractable = true;
                }

                if (!isInteractable) continue;

                auto& rect = m_cm->GetComponent<UIRectTransform>(e);

                // Check if this entity belongs to this canvas
                Entity root = e;
                Entity current = m_cm->HasComponent<Hierarchy>(e) ? m_cm->GetComponent<Hierarchy>(e).parent : NO_ENTITY;
                while (current != NO_ENTITY) {
                    root = current;
                    if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                    current = m_cm->HasComponent<Hierarchy>(current) ? m_cm->GetComponent<Hierarchy>(current).parent : NO_ENTITY;
                }

                Entity parentEnt = m_cm->HasComponent<Hierarchy>(e) ? m_cm->GetComponent<Hierarchy>(e).parent : NO_ENTITY;
                if (root != canvasEntity && parentEnt != canvasEntity) {
                    continue;
                }

                // Calculate world rect
                float worldX, worldY, worldWidth, worldHeight;
                CalculateWorldRect(e, canvasEntity, *canvasPtr, worldX, worldY, worldWidth, worldHeight);

                UIElementInfo info;
                info.entity = e;
                info.worldX = worldX;
                info.worldY = worldY;
                info.worldWidth = worldWidth;
                info.worldHeight = worldHeight;
                info.zOrder = rect.z + canvasPtr->sortingOrder * 1000.0f;
                info.canvasEntity = canvasEntity;
                // Store accumulated rotation and pivot for rotation-aware hit testing
                info.rotationZ = rect.cachedWorldRotZ;
                info.pivotX = rect.pivotX;
                info.pivotY = rect.pivotY;

                elements.push_back(info);
            }
        }

        return elements;
    }

    bool UIEventSystem::PointInRect(float px, float py, const UIElementInfo& element) {
        // Helper: normalize bounds so negative sizes (from negative scale) work correctly
        auto inBounds = [](float p, float origin, float size) {
            float lo = std::min(origin, origin + size);
            float hi = std::max(origin, origin + size);
            return p >= lo && p <= hi;
        };

        // Handle rotation by transforming point into the element's local space before AABB check
        if (std::abs(element.rotationZ) > 0.001f) {
            // Center of element based on pivot
            float centerX = element.worldX + element.worldWidth  * element.pivotX;
            float centerY = element.worldY + element.worldHeight * element.pivotY;

            // Translate point relative to center
            float dx = px - centerX;
            float dy = py - centerY;

            // Rotate point back by -rotationZ to align with element's local (unrotated) space
            float rad = element.rotationZ * PI / 180.0f;
            float cos_a = std::cos(-rad);
            float sin_a = std::sin(-rad);
            float localX = dx * cos_a - dy * sin_a + centerX;
            float localY = dx * sin_a + dy * cos_a + centerY;

            // AABB check in local space (handles negative scale)
            return inBounds(localX, element.worldX, element.worldWidth) &&
                   inBounds(localY, element.worldY, element.worldHeight);
        }

        // No rotation: simple AABB (handles negative scale)
        return inBounds(px, element.worldX, element.worldWidth) &&
               inBounds(py, element.worldY, element.worldHeight);
    }

    void UIEventSystem::UpdateButtonStates() {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIButton>(e)) continue;
            Entity canvasEntity = FindOwningCanvas(e);
            if (canvasEntity != NO_ENTITY) {
                if (!IsActiveForUI(e, canvasEntity)) continue;
            }
            else {
                // If it's not on a canvas, still respect its EntityMeta
                //if (m_cm->HasComponent<NE::ECS::Component::EntityMeta>(e) &&
                //    !m_cm->GetComponent<NE::ECS::Component::EntityMeta>(e).isActive) {
                //    continue;
                //}
                if (!m_em->GetActive(e)) continue;
            }

            auto& button = m_cm->GetComponent<UIButton>(e);
            UIButton::State newState = button.currentState;

            if (!button.interactable) {
                newState = UIButton::State::DISABLED;
            } else if (m_pressedEntity == e) {
                newState = UIButton::State::PRESSED;
            } else if (m_hoveredEntity == e) {
                newState = UIButton::State::HOVERED;
            } else {
                newState = UIButton::State::NORMAL;
            }

            // Detect click on state transition
            if (button.previousState == UIButton::State::PRESSED &&
                newState == UIButton::State::NORMAL) {
                // This was a click (handled in Update via wasClicked flag)
            }

            button.previousState = button.currentState;
            button.currentState = newState;

            // Apply button color to sibling UIImage
            ApplyButtonColorToImage(e);
        }
    }

    void UIEventSystem::ApplyButtonColorToImage(Entity buttonEntity) {
        if (!m_cm->HasComponent<UIButton>(buttonEntity)) return;
        if (!m_cm->HasComponent<UIImage>(buttonEntity)) return;

        auto& button = m_cm->GetComponent<UIButton>(buttonEntity);
        auto& image = m_cm->GetComponent<UIImage>(buttonEntity);

        switch (button.currentState) {
            case UIButton::State::NORMAL:
                image.color = button.normalColor;
                break;
            case UIButton::State::HOVERED:
                image.color = button.hoverColor;
                break;
            case UIButton::State::PRESSED:
                image.color = button.pressedColor;
                break;
            case UIButton::State::DISABLED:
                image.color = button.disabledColor;
                break;
        }
    }

    void UIEventSystem::UpdateSliderStates(float mouseX, float mouseY, bool mouseDown, bool mousePressed, bool mouseReleased) {
        const auto& entities = GetEntities();

        // Handle mouse press on slider
        if (mousePressed && m_pressedEntity != NO_ENTITY) {
            if (m_cm->HasComponent<UISlider>(m_pressedEntity)) {
                auto& slider = m_cm->GetComponent<UISlider>(m_pressedEntity);
                if (slider.interactable) {
                    m_draggingSlider = m_pressedEntity;
                    slider.isDragging = true;
                }
            }
        }

        // Handle mouse release
        if (mouseReleased && m_draggingSlider != NO_ENTITY) {
            if (m_cm->HasComponent<UISlider>(m_draggingSlider)) {
                auto& slider = m_cm->GetComponent<UISlider>(m_draggingSlider);
                slider.isDragging = false;
            }
            m_draggingSlider = NO_ENTITY;
        }

        // Handle slider drag: update value from mouse position
        if (mouseDown && m_draggingSlider != NO_ENTITY) {
            if (m_cm->HasComponent<UISlider>(m_draggingSlider) &&
                m_cm->HasComponent<UIRectTransform>(m_draggingSlider)) {

                auto& slider = m_cm->GetComponent<UISlider>(m_draggingSlider);

                // Find canvas for this slider
                Entity canvasEntity = NO_ENTITY;
                Entity current = m_cm->HasComponent<Hierarchy>(m_draggingSlider) ? m_cm->GetComponent<Hierarchy>(m_draggingSlider).parent : NO_ENTITY;
                while (current != NO_ENTITY) {
                    if (m_cm->HasComponent<UICanvas>(current)) {
                        canvasEntity = current;
                        break;
                    }
                    if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                    current = m_cm->HasComponent<Hierarchy>(current) ? m_cm->GetComponent<Hierarchy>(current).parent : NO_ENTITY;
                }

                if (canvasEntity != NO_ENTITY && m_cm->HasComponent<UICanvas>(canvasEntity)) {
                    auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);
                    float worldX, worldY, worldWidth, worldHeight;
                    CalculateWorldRect(m_draggingSlider, canvasEntity, canvas, worldX, worldY, worldWidth, worldHeight);

                    // Calculate normalized value based on mouse position
                    float normalized = 0.0f;
                    if (slider.IsHorizontal()) {
                        if (worldWidth > 0.0f) {
                            normalized = (mouseX - worldX) / worldWidth;
                        }
                    } else {
                        // Y increases downward in screen space, so flip for natural bottom=0, top=1
                        if (worldHeight > 0.0f) {
                            normalized = 1.0f - (mouseY - worldY) / worldHeight;
                        }
                    }

                    // Reverse if needed
                    if (slider.IsReversed()) {
                        normalized = 1.0f - normalized;
                    }

                    // Clamp and set value
                    float oldValue = slider.value;
                    slider.SetNormalizedValue(normalized);

                    // Check if value changed
                    if (slider.value != oldValue) {
                        slider.valueChanged = true;
                        NANOEngine::Events::EventBus::Get().Dispatch(
                            NANOEngine::Events::EventDomain::Engine,
                            NANOEngine::Events::UISliderValueChangedEvent{ m_draggingSlider, slider.value, oldValue }
                        );
                    }
                }
            }
        }

        // Every frame: sync fill rect and handle position for ALL sliders based on current value
        for (Entity e : entities) {
            if (!m_cm->HasComponent<UISlider>(e)) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;

            auto& slider = m_cm->GetComponent<UISlider>(e);
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            float fillNormalized = slider.GetNormalizedValue();
            float handleNormalized = fillNormalized;

            // Update fill rect based on direction.
            // In the layout engine, child x=0 is at the parent's CENTER (anchor=0.5).
            // To place a child's left edge at the parent's left edge: x = -parentWidth + childWidth*0.5
            // To place a child's right edge at the parent's right edge: x = -childWidth*0.5
            if (slider.fillRect != UINT32_MAX && m_cm->HasComponent<UIRectTransform>(slider.fillRect)) {
                auto& fillRect = m_cm->GetComponent<UIRectTransform>(slider.fillRect);

                if (slider.IsHorizontal()) {
                    fillRect.width = rect.width * fillNormalized;
                    fillRect.y = -rect.height * 0.5f;  // Center fill vertically on parent
                    if (!slider.IsReversed()) {
                        // LEFT_TO_RIGHT: fill grows from left edge
                        fillRect.x = -rect.width + fillRect.width * 0.5f;
                    } else {
                        // RIGHT_TO_LEFT: fill grows from right edge
                        fillRect.x = -fillRect.width * 0.5f;
                    }
                } else {
                    fillRect.height = rect.height * fillNormalized;
                    fillRect.x = -rect.width * 0.5f;  // Center fill horizontally on parent
                    if (!slider.IsReversed()) {
                        // BOTTOM_TO_TOP: fill grows from bottom edge (y increases downward, bottom = +height/2)
                        fillRect.y = -fillRect.height * 0.5f;
                    } else {
                        // TOP_TO_BOTTOM: fill grows from top edge
                        fillRect.y = -rect.height + fillRect.height * 0.5f;
                    }
                }
            }

            // Update handle position along the track.
            // At normalized=0 for non-reversed: handle at left/bottom edge.
            // At normalized=1 for non-reversed: handle at right/top edge.
            if (slider.handleRect != UINT32_MAX && m_cm->HasComponent<UIRectTransform>(slider.handleRect)) {
                auto& handleRect = m_cm->GetComponent<UIRectTransform>(slider.handleRect);

                if (slider.IsHorizontal()) {
                    float trackWidth = rect.width - handleRect.width;
                    handleRect.y = -rect.height * 0.5f;  // Center handle vertically on parent
                    if (!slider.IsReversed()) {
                        // LEFT_TO_RIGHT: handle moves left→right
                        handleRect.x = -rect.width + handleRect.width * 0.5f + trackWidth * handleNormalized;
                    } else {
                        // RIGHT_TO_LEFT: handle moves right→left
                        handleRect.x = -handleRect.width * 0.5f - trackWidth * handleNormalized;
                    }
                } else {
                    float trackHeight = rect.height - handleRect.height;
                    handleRect.x = -rect.width * 0.5f;  // Center handle horizontally on parent
                    if (!slider.IsReversed()) {
                        // BOTTOM_TO_TOP: handle moves bottom→top (more negative y = higher)
                        handleRect.y = -handleRect.height * 0.5f - trackHeight * handleNormalized;
                    } else {
                        // TOP_TO_BOTTOM: handle moves top→bottom
                        handleRect.y = -rect.height + handleRect.height * 0.5f + trackHeight * handleNormalized;
                    }
                }
            }
        }
    }

    void UIEventSystem::UpdateToggleStates() {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIToggle>(e)) continue;

            auto& toggle = m_cm->GetComponent<UIToggle>(e);

            // Check if toggle (or its background) was clicked
            bool clicked = false;

            // Check if the toggle entity itself was clicked
            if (m_pressedEntity == e && m_hoveredEntity == e) {
                // Release check handled in Update()
            }

            // Check if the toggle's background was clicked
            if (toggle.background != UINT32_MAX) {
                if (m_cm->HasComponent<UIButton>(toggle.background)) {
                    auto& bgButton = m_cm->GetComponent<UIButton>(toggle.background);
                    if (bgButton.wasClicked) {
                        clicked = true;
                    }
                }
            }

            // Handle the click
            if (clicked && toggle.interactable) {
                toggle.Toggle();
                UpdateCheckmarkVisibility(e);
            }

            // Always ensure checkmark visibility is correct
            UpdateCheckmarkVisibility(e);
        }
    }

    void UIEventSystem::UpdateCheckmarkVisibility(Entity toggleEntity) {
        if (!m_cm->HasComponent<UIToggle>(toggleEntity)) return;

        auto& toggle = m_cm->GetComponent<UIToggle>(toggleEntity);

        // Show/hide checkmark based on isOn state
        if (toggle.graphic != UINT32_MAX && m_cm->HasComponent<UIImage>(toggle.graphic)) {
            auto& checkmark = m_cm->GetComponent<UIImage>(toggle.graphic);
            // Use alpha to show/hide
            checkmark.color.w = toggle.isOn ? 1.0f : 0.0f;
        }
    }

    float UIEventSystem::CalculateScaleFactor(const UICanvas& canvas) {
        return UIUtil::CalculateScaleFactor(canvas);
    }

    void UIEventSystem::CalculateWorldRect(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        float& outX, float& outY,
        float& outWidth, float& outHeight
    ) {
        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            outX = outY = outWidth = outHeight = 0.0f;
            return;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        // Use cached values from UIRenderSystem if available (avoids re-traversal)
        if (rect.worldRectCached) {
            outX = rect.cachedWorldX;
            outY = rect.cachedWorldY;
            outWidth = rect.cachedWorldWidth;
            outHeight = rect.cachedWorldHeight;
            return;
        }

        // For WorldSpace, the fallback screen-space logic doesn't apply.
        // Use zero rect (one-frame lag on first frame is acceptable).
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            outX = outY = outWidth = outHeight = 0.0f;
            return;
        }

        // Fallback: compute from scratch (handles entities not processed by UIRenderSystem)
        float scaleFactor = CalculateScaleFactor(canvas);

        // Build parent chain (excluding canvas for screen space)
        std::vector<Entity> chain;
        Entity current = entity;
        while (current != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(current)) {
            if (current == canvasEntity) break;
            chain.push_back(current);
            current = m_cm->HasComponent<Hierarchy>(current) ? m_cm->GetComponent<Hierarchy>(current).parent : NO_ENTITY;
        }
        std::reverse(chain.begin(), chain.end());

        // Accumulate transforms
        float accScaleX = scaleFactor;
        float accScaleY = scaleFactor;
        float accPosX = 0.0f;
        float accPosY = 0.0f;
        float accRotationZ = 0.0f;

        for (size_t i = 0; i < chain.size(); ++i) {
            Entity ent = chain[i];
            auto& r = m_cm->GetComponent<UIRectTransform>(ent);
            bool isTarget = (ent == entity);

            accScaleX *= r.scaleX;
            accScaleY *= r.scaleY;
            accRotationZ += r.rotationZ;

            float parentWidth, parentHeight;
            if (i > 0) {
                Entity parentEntity = chain[i - 1];
                auto& parentRect = m_cm->GetComponent<UIRectTransform>(parentEntity);
                parentWidth = parentRect.width;
                parentHeight = parentRect.height;
            } else {
                parentWidth = NE::Graphics::GraphicsManager::GetScreenWidth() / scaleFactor;
                parentHeight = NE::Graphics::GraphicsManager::GetScreenHeight() / scaleFactor;
            }

            float anchorX = parentWidth * DEFAULT_ANCHOR_X;
            float anchorY = parentHeight * DEFAULT_ANCHOR_Y;

            if (isTarget) {
                float scaledWidth = r.width * accScaleX;
                float scaledHeight = r.height * accScaleY;

                float localX = anchorX + r.x - scaledWidth * r.pivotX;
                float localY = anchorY + r.y - scaledHeight * r.pivotY;

                // Apply parent rotation if present
                float parentRotation = accRotationZ - r.rotationZ;
                if (std::abs(parentRotation) > 0.001f) {
                    float rad = parentRotation * PI / 180.0f;
                    float cosR = std::cos(rad);
                    float sinR = std::sin(rad);
                    float rotatedX = localX * cosR - localY * sinR;
                    float rotatedY = localX * sinR + localY * cosR;
                    localX = rotatedX;
                    localY = rotatedY;
                }

                float parentScaleX = accScaleX / r.scaleX;
                float parentScaleY = accScaleY / r.scaleY;

                accPosX += localX * parentScaleX;
                accPosY += localY * parentScaleY;
            } else {
                accPosX += anchorX + r.x;
                accPosY += anchorY + r.y;
            }
        }

        outX = accPosX;
        outY = accPosY;
        outWidth = rect.width * accScaleX;
        outHeight = rect.height * accScaleY;
    }

    //=========================================================================
    // World-Space Hit Testing
    //=========================================================================

    bool UIEventSystem::GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj) {
        auto* cam = NE::Graphics::GraphicsManager::GetEditorCamera();
        if (!cam) return false;

        outView = cam->GetViewMatrix();
        outProj = cam->GetProjectionMatrix();
        return true;
    }

    Math::Vec3 UIEventSystem::ScreenToWorldRay(float screenX, float screenY, const Math::Mat4& invViewProj) {
        float screenWidth = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenWidth());
        float screenHeight = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenHeight());

        // Convert screen coords to NDC (-1 to 1)
        float ndcX = (2.0f * screenX / screenWidth) - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY / screenHeight);  // Flip Y

        // Create near and far points in NDC
        Math::Vec4 nearPoint(ndcX, ndcY, -1.0f, 1.0f);
        Math::Vec4 farPoint(ndcX, ndcY, 1.0f, 1.0f);

        // Transform to world space
        Math::Vec4 nearWorld = invViewProj * nearPoint;
        Math::Vec4 farWorld = invViewProj * farPoint;

        // Perspective divide
        if (std::abs(nearWorld.w) > 0.0001f) {
            nearWorld.x /= nearWorld.w;
            nearWorld.y /= nearWorld.w;
            nearWorld.z /= nearWorld.w;
        }
        if (std::abs(farWorld.w) > 0.0001f) {
            farWorld.x /= farWorld.w;
            farWorld.y /= farWorld.w;
            farWorld.z /= farWorld.w;
        }

        // Ray direction
        Math::Vec3 rayDir(
            farWorld.x - nearWorld.x,
            farWorld.y - nearWorld.y,
            farWorld.z - nearWorld.z
        );

        return rayDir.Normalized();
    }

    bool UIEventSystem::RayPlaneIntersect(
        const Math::Vec3& rayOrigin,
        const Math::Vec3& rayDir,
        const Math::Vec3& planePoint,
        const Math::Vec3& planeNormal,
        float& outT,
        Math::Vec3& outHitPoint
    ) {
        float denom = planeNormal.Dot(rayDir);

        // Check if ray is parallel to plane
        if (std::abs(denom) < 0.0001f) {
            return false;
        }

        Math::Vec3 diff = planePoint - rayOrigin;
        outT = diff.Dot(planeNormal) / denom;

        // Check if intersection is behind the ray origin
        if (outT < 0.0f) {
            return false;
        }

        outHitPoint = rayOrigin + rayDir * outT;
        return true;
    }

    Math::Mat4 UIEventSystem::BuildWorldSpaceModelMatrix(
        Entity entity,
        Entity canvasEntity,
        const Component::UIRectTransform& rect
    ) {
        // If UIRenderSystem already computed the world matrix this frame, reuse it
        if (!rect.worldMatrixDirty) {
            return rect.worldMatrix;
        }

        // Fallback: compute from scratch (handles entities not processed by UIRenderSystem)
        // Build parent chain including canvas
        std::vector<Entity> chain;
        Entity current = entity;

        while (current != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(current)) {
            chain.push_back(current);
            current = m_cm->HasComponent<Hierarchy>(current) ? m_cm->GetComponent<Hierarchy>(current).parent : NO_ENTITY;
        }

        std::reverse(chain.begin(), chain.end());

        // Accumulate transforms
        float accScaleX = 1.0f;
        float accScaleY = 1.0f;
        float accScaleZ = 1.0f;
        float accPosX = 0.0f;
        float accPosY = 0.0f;
        float accPosZ = 0.0f;
        float accRotX = 0.0f;
        float accRotY = 0.0f;
        float accRotZ = 0.0f;

        for (Entity ent : chain) {
            auto& r = m_cm->GetComponent<UIRectTransform>(ent);

            // Position is in parent's local space — scale by accumulated parent scale
            accPosX += r.x * accScaleX;
            accPosY += r.y * accScaleY;
            accPosZ += r.z * accScaleZ;

            // Then apply this entity's own scale
            accScaleX *= r.scaleX;
            accScaleY *= r.scaleY;
            accScaleZ *= r.scaleZ;
            accRotX += r.rotationX;
            accRotY += r.rotationY;
            accRotZ += r.rotationZ;
        }

        // Build matrices — must match UILayoutEngine::UpdateWorldMatrixFromAccumulated (WorldSpace path)
        // Scale includes element dimensions (unit quad → world-sized element)
        Math::Mat4 scaleMatrix = Math::Mat4::BuildScaling(
            rect.width * accScaleX,
            rect.height * accScaleY,
            accScaleZ
        );

        // Pivot in unit space (0..1), applied before scale
        float unitPivotX = rect.pivotX;
        float unitPivotY = rect.pivotY;
        Math::Mat4 pivotTrans    = Math::Mat4::BuildTranslation( unitPivotX,  unitPivotY, 0.0f);
        Math::Mat4 pivotTransInv = Math::Mat4::BuildTranslation(-unitPivotX, -unitPivotY, 0.0f);

        // Build*Rotation takes degrees — no conversion needed
        Math::Mat4 rotationX = Math::Mat4::BuildXRotation(accRotX);
        Math::Mat4 rotationY = Math::Mat4::BuildYRotation(accRotY);
        Math::Mat4 rotationZ = Math::Mat4::BuildZRotation(accRotZ);
        Math::Mat4 rotationMatrix = rotationZ * rotationY * rotationX;

        Math::Mat4 translationMatrix = Math::Mat4::BuildTranslation(accPosX, accPosY, accPosZ);

        // Order: offset pivot in unit space → rotate → restore pivot → scale to world → translate
        return translationMatrix * scaleMatrix * pivotTrans * rotationMatrix * pivotTransInv;
    }

    bool UIEventSystem::IsPointInWorldSpaceElement(
        const Math::Vec3& hitPoint,
        const UIElementInfo& element
    ) {
        // Get the inverse model matrix to transform hit point to local space
        Math::Mat4 invModel = element.modelMatrix.Inverse();
        Math::Vec4 localPoint4 = invModel * Math::Vec4(hitPoint.x, hitPoint.y, hitPoint.z, 1.0f);

        // Local coordinates (in unit quad space: 0-1)
        float localX = localPoint4.x;
        float localY = localPoint4.y;

        // Check if within bounds (unit quad: 0 to 1)
        return localX >= 0.0f && localX <= 1.0f && localY >= 0.0f && localY <= 1.0f;
    }

    std::vector<UIEventSystem::UIElementInfo> UIEventSystem::CollectWorldSpaceElements() {
        std::vector<UIElementInfo> elements;
        const auto& allEntities = GetEntities();

        // Find world-space canvases
        std::vector<std::pair<Entity, UICanvas*>> worldCanvases;
        for (Entity e : allEntities) {
            if (m_cm->HasComponent<UICanvas>(e)) {
                auto& canvas = m_cm->GetComponent<UICanvas>(e);
                bool metaActive = m_em->GetActive(e);
                if (canvas.isActive && metaActive && canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                    worldCanvases.push_back({ e, &canvas });
                }
            }
        }

        // For each world-space canvas, collect interactable elements
        for (const auto& [canvasEntity, canvasPtr] : worldCanvases) {
            for (Entity e : allEntities) {
                if (e == canvasEntity) continue;
                if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
                if (!IsActiveForUI(e, canvasEntity)) continue;

                // Check if entity is interactable
                bool isInteractable = false;

                if (m_cm->HasComponent<UIButton>(e)) {
                    auto& button = m_cm->GetComponent<UIButton>(e);
                    if (button.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UISlider>(e)) {
                    auto& slider = m_cm->GetComponent<UISlider>(e);
                    if (slider.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UIToggle>(e)) {
                    auto& toggle = m_cm->GetComponent<UIToggle>(e);
                    if (toggle.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UIImage>(e)) {
                    auto& image = m_cm->GetComponent<UIImage>(e);
                    if (image.raycastTarget) isInteractable = true;
                }
                if (m_cm->HasComponent<UIInputField>(e)) {
                    auto& inputField = m_cm->GetComponent<UIInputField>(e);
                    if (inputField.interactable) isInteractable = true;
                }
                if (m_cm->HasComponent<UIDropdown>(e)) {
                    auto& dropdown = m_cm->GetComponent<UIDropdown>(e);
                    if (dropdown.interactable) isInteractable = true;
                }

                if (!isInteractable) continue;

                auto& rect = m_cm->GetComponent<UIRectTransform>(e);

                // Check if this entity belongs to this canvas
                Entity root = e;
                Entity current = m_cm->HasComponent<Hierarchy>(e) ? m_cm->GetComponent<Hierarchy>(e).parent : NO_ENTITY;
                while (current != NO_ENTITY) {
                    root = current;
                    if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                    current = m_cm->HasComponent<Hierarchy>(current) ? m_cm->GetComponent<Hierarchy>(current).parent : NO_ENTITY;
                }

                Entity parentEnt = m_cm->HasComponent<Hierarchy>(e) ? m_cm->GetComponent<Hierarchy>(e).parent : NO_ENTITY;
                if (root != canvasEntity && parentEnt != canvasEntity) {
                    continue;
                }

                // Build model matrix for this element
                Math::Mat4 modelMatrix = BuildWorldSpaceModelMatrix(e, canvasEntity, rect);

                // Extract world position from model matrix (column 3)
                Math::Vec3 worldPos(
                    modelMatrix.GetElement(0, 3),
                    modelMatrix.GetElement(1, 3),
                    modelMatrix.GetElement(2, 3)
                );

                // Get plane normal (Z axis of model matrix, typically facing +Z or -Z)
                Math::Vec3 worldNormal(
                    modelMatrix.GetElement(0, 2),
                    modelMatrix.GetElement(1, 2),
                    modelMatrix.GetElement(2, 2)
                );
                worldNormal.Normalize();

                UIElementInfo info;
                info.entity = e;
                info.canvasEntity = canvasEntity;
                info.isWorldSpace = true;
                info.worldPosition = worldPos;
                info.worldNormal = worldNormal;
                info.modelMatrix = modelMatrix;
                info.zOrder = rect.z + canvasPtr->sortingOrder * 1000.0f;

                elements.push_back(info);
            }
        }

        return elements;
    }

    Entity UIEventSystem::FindWorldSpaceHit(float mouseX, float mouseY) {
        Math::Mat4 viewMatrix, projMatrix;
        if (!GetCameraMatrices(viewMatrix, projMatrix)) {
            return NO_ENTITY;
        }

        // Get camera position
        auto* cam = NE::Graphics::GraphicsManager::GetEditorCamera();
        if (!cam) return NO_ENTITY;

        Math::Vec3 rayOrigin = cam->GetPosition();

        // Compute inverse view-projection matrix
        Math::Mat4 viewProj = projMatrix * viewMatrix;
        Math::Mat4 invViewProj = viewProj.Inverse();

        // Get ray direction
        Math::Vec3 rayDir = ScreenToWorldRay(mouseX, mouseY, invViewProj);

        // Collect world-space elements
        std::vector<UIElementInfo> worldElements = CollectWorldSpaceElements();

        if (worldElements.empty()) {
            return NO_ENTITY;
        }

        // Find closest hit
        Entity closestEntity = NO_ENTITY;
        float closestT = std::numeric_limits<float>::max();

        for (const auto& elem : worldElements) {
            // Back-face rejection: skip if ray hits the back of the canvas plane
            float nDotR = elem.worldNormal.Dot(rayDir);
            if (nDotR >= 0.f) continue;

            float t;
            Math::Vec3 hitPoint;

            if (RayPlaneIntersect(rayOrigin, rayDir, elem.worldPosition, elem.worldNormal, t, hitPoint)) {
                if (t < closestT && IsPointInWorldSpaceElement(hitPoint, elem)) {
                    closestT = t;
                    closestEntity = elem.entity;
                }
            }
        }

        return closestEntity;
    }

    //=========================================================================
    // Viewport Transform (for Editor viewports)
    //=========================================================================

    void UIEventSystem::SetViewportBounds(float offsetX, float offsetY, float width, float height, float uiWidth, float uiHeight) {
        s_useViewportTransform = true;
        s_viewportOffsetX = offsetX;
        s_viewportOffsetY = offsetY;
        s_viewportWidth = width;
        s_viewportHeight = height;
        s_uiWidth = uiWidth;
        s_uiHeight = uiHeight;
    }

    void UIEventSystem::ClearViewportBounds() {
        s_useViewportTransform = false;
    }

    void UIEventSystem::TransformMouseToUICoords(double& mouseX, double& mouseY) {
        if (s_useViewportTransform) {
            // Transform from window coords to viewport-relative coords
            double localX = mouseX - s_viewportOffsetX;
            double localY = mouseY - s_viewportOffsetY;

            // Normalize to 0-1
            double normX = localX / s_viewportWidth;
            double normY = localY / s_viewportHeight;

            // Scale to UI coords
            mouseX = normX * s_uiWidth;
            mouseY = normY * s_uiHeight;
        }
    }

    //=========================================================================
    // Mask-Aware Hit Testing (RectMask2D)
    //=========================================================================

    bool UIEventSystem::IsPointInMaskBounds(Entity entity, float px, float py) {
        Entity current = m_cm->HasComponent<Hierarchy>(entity)
            ? m_cm->GetComponent<Hierarchy>(entity).parent
            : NO_ENTITY;

        while (current != NO_ENTITY) {
            if (m_cm->HasComponent<UIRectTransform>(current)) {
                auto& maskRect = m_cm->GetComponent<UIRectTransform>(current);
                if (maskRect.enableMask) {
                    Entity canvasEntity = FindOwningCanvas(current);
                    if (canvasEntity != NO_ENTITY && m_cm->HasComponent<UICanvas>(canvasEntity)) {
                        auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);
                        float mx, my, mw, mh;
                        CalculateWorldRect(current, canvasEntity, canvas, mx, my, mw, mh);

                        // Apply mask padding
                        mx += maskRect.maskPaddingLeft;
                        my += maskRect.maskPaddingTop;
                        mw -= maskRect.maskPaddingLeft + maskRect.maskPaddingRight;
                        mh -= maskRect.maskPaddingTop + maskRect.maskPaddingBottom;

                        if (px < mx || px > mx + mw || py < my || py > my + mh) {
                            return false;
                        }
                    }
                }
            }

            if (!m_cm->HasComponent<Hierarchy>(current)) break;
            current = m_cm->GetComponent<Hierarchy>(current).parent;
        }

        return true;
    }

    //=========================================================================
    // ScrollRect
    //=========================================================================

    void UIEventSystem::UpdateScrollRects(
        float mouseX, float mouseY,
        bool mouseDown, bool mousePressed, bool mouseReleased,
        double deltaTime
    ) {
        float dt = static_cast<float>(deltaTime);
        if (dt <= 0.f) dt = 1.f / 60.f;

        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIScrollRect>(e)) continue;

            auto& scroll = m_cm->GetComponent<UIScrollRect>(e);
            if (!scroll.interactable) continue;

            // Get viewport entity and its world rect
            Entity viewportEnt = scroll.viewportEntity;
            if (viewportEnt == UINT32_MAX || !m_cm->HasComponent<UIRectTransform>(viewportEnt)) continue;

            Entity contentEnt = scroll.contentEntity;
            if (contentEnt == UINT32_MAX || !m_cm->HasComponent<UIRectTransform>(contentEnt)) continue;

            Entity canvasEntity = FindOwningCanvas(e);
            if (canvasEntity == NO_ENTITY) continue;
            auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);

            float vpX, vpY, vpW, vpH;
            CalculateWorldRect(viewportEnt, canvasEntity, canvas, vpX, vpY, vpW, vpH);

            auto& contentRect = m_cm->GetComponent<UIRectTransform>(contentEnt);

            // Cache viewport dimensions
            scroll.viewportWidth = vpW;
            scroll.viewportHeight = vpH;

            // Calculate content bounds from children
            if (m_layoutEngine) {
                auto bounds = m_layoutEngine->CalculateContentBounds(contentEnt);
                scroll.contentWidth = std::max(bounds.width, contentRect.width);
                scroll.contentHeight = std::max(bounds.height, contentRect.height);
            } else {
                scroll.contentWidth = contentRect.width;
                scroll.contentHeight = contentRect.height;
            }

            bool mouseInViewport = (mouseX >= vpX && mouseX <= vpX + vpW &&
                                    mouseY >= vpY && mouseY <= vpY + vpH);

            // Handle drag start
            if (mousePressed && mouseInViewport && m_draggingScrollRect == NO_ENTITY) {
                m_draggingScrollRect = e;
                scroll.isDragging = true;
                scroll.dragStartX = mouseX;
                scroll.dragStartY = mouseY;
                scroll.contentStartX = contentRect.x;
                scroll.contentStartY = contentRect.y;
                scroll.velocity = Math::Vec2(0.f, 0.f);
            }

            // Handle drag
            if (scroll.isDragging && m_draggingScrollRect == e && mouseDown) {
                float deltaX = mouseX - scroll.dragStartX;
                float deltaY = mouseY - scroll.dragStartY;

                float newX = scroll.contentStartX + (scroll.horizontal ? deltaX : 0.f);
                float newY = scroll.contentStartY + (scroll.vertical ? deltaY : 0.f);

                // Calculate scroll bounds
                float maxScrollX = std::max(0.f, scroll.contentWidth - scroll.viewportWidth);
                float maxScrollY = std::max(0.f, scroll.contentHeight - scroll.viewportHeight);

                // The content starts at the center of the viewport, so min scroll offset
                // is when content top-left aligns with viewport top-left
                float halfVpW = scroll.viewportWidth * 0.5f;
                float halfVpH = scroll.viewportHeight * 0.5f;
                float minX = -maxScrollX + halfVpW - scroll.contentWidth * contentRect.pivotX;
                float maxX = halfVpW - scroll.contentWidth * contentRect.pivotX;
                float minY = -maxScrollY + halfVpH - scroll.contentHeight * contentRect.pivotY;
                float maxY = halfVpH - scroll.contentHeight * contentRect.pivotY;

                if (scroll.movementType == UIScrollRect::MovementType::Clamped) {
                    if (scroll.horizontal) newX = std::max(minX, std::min(maxX, newX));
                    if (scroll.vertical) newY = std::max(minY, std::min(maxY, newY));
                }

                if (scroll.horizontal) contentRect.x = newX;
                if (scroll.vertical) contentRect.y = newY;

                // Track velocity for inertia
                if (dt > 0.f) {
                    scroll.velocity.x = (scroll.horizontal ? deltaX : 0.f) / dt;
                    scroll.velocity.y = (scroll.vertical ? deltaY : 0.f) / dt;
                }

                // Update drag start for next frame delta
                scroll.dragStartX = mouseX;
                scroll.dragStartY = mouseY;
                scroll.contentStartX = contentRect.x;
                scroll.contentStartY = contentRect.y;

                contentRect.worldMatrixDirty = true;
                contentRect.worldRectCached = false;
            }

            // Handle drag end
            if (scroll.isDragging && m_draggingScrollRect == e && mouseReleased) {
                scroll.isDragging = false;
                m_draggingScrollRect = NO_ENTITY;
                if (!scroll.inertia) {
                    scroll.velocity = Math::Vec2(0.f, 0.f);
                }
            }

            // Handle mouse wheel scrolling
            if (!scroll.isDragging && mouseInViewport) {
                auto [wheelX, wheelY] = NE::InputManager::ScrollDelta();
                if (std::abs(wheelX) > 0.0 || std::abs(wheelY) > 0.0) {
                    float maxScrollX = std::max(0.f, scroll.contentWidth - scroll.viewportWidth);
                    float maxScrollY = std::max(0.f, scroll.contentHeight - scroll.viewportHeight);
                    float halfVpW = scroll.viewportWidth * 0.5f;
                    float halfVpH = scroll.viewportHeight * 0.5f;
                    float minX = -maxScrollX + halfVpW - scroll.contentWidth * contentRect.pivotX;
                    float maxX = halfVpW - scroll.contentWidth * contentRect.pivotX;
                    float minY = -maxScrollY + halfVpH - scroll.contentHeight * contentRect.pivotY;
                    float maxY = halfVpH - scroll.contentHeight * contentRect.pivotY;

                    if (scroll.horizontal && std::abs(wheelX) > 0.0) {
                        contentRect.x += static_cast<float>(wheelX) * scroll.scrollSensitivity * 10.f;
                        if (scroll.movementType == UIScrollRect::MovementType::Clamped) {
                            contentRect.x = std::max(minX, std::min(maxX, contentRect.x));
                        }
                    }
                    if (scroll.vertical && std::abs(wheelY) > 0.0) {
                        contentRect.y += static_cast<float>(wheelY) * scroll.scrollSensitivity * 10.f;
                        if (scroll.movementType == UIScrollRect::MovementType::Clamped) {
                            contentRect.y = std::max(minY, std::min(maxY, contentRect.y));
                        }
                    }

                    // Zero velocity so wheel input doesn't combine with inertia
                    scroll.velocity = Math::Vec2(0.f, 0.f);

                    contentRect.worldMatrixDirty = true;
                    contentRect.worldRectCached = false;
                }
            }

            // Apply inertia (when not dragging)
            if (!scroll.isDragging && (std::abs(scroll.velocity.x) > 0.1f || std::abs(scroll.velocity.y) > 0.1f)) {
                if (scroll.horizontal) contentRect.x += scroll.velocity.x * dt;
                if (scroll.vertical) contentRect.y += scroll.velocity.y * dt;

                // Decay velocity (framerate-independent)
                float decay = std::pow(scroll.decelerationRate, dt);
                scroll.velocity.x *= decay;
                scroll.velocity.y *= decay;

                contentRect.worldMatrixDirty = true;
                contentRect.worldRectCached = false;
            }

            // Elastic bounce-back (when content is out of bounds and movementType == Elastic)
            if (!scroll.isDragging && scroll.movementType == UIScrollRect::MovementType::Elastic) {
                float maxScrollX = std::max(0.f, scroll.contentWidth - scroll.viewportWidth);
                float maxScrollY = std::max(0.f, scroll.contentHeight - scroll.viewportHeight);
                float halfVpW = scroll.viewportWidth * 0.5f;
                float halfVpH = scroll.viewportHeight * 0.5f;
                float minX = -maxScrollX + halfVpW - scroll.contentWidth * contentRect.pivotX;
                float maxX2 = halfVpW - scroll.contentWidth * contentRect.pivotX;
                float minY = -maxScrollY + halfVpH - scroll.contentHeight * contentRect.pivotY;
                float maxY2 = halfVpH - scroll.contentHeight * contentRect.pivotY;

                float springFactor = 1.f - std::pow(scroll.elasticity, dt);

                if (scroll.horizontal) {
                    if (contentRect.x < minX) {
                        contentRect.x += (minX - contentRect.x) * springFactor;
                        scroll.velocity.x = 0.f;
                        contentRect.worldMatrixDirty = true;
                        contentRect.worldRectCached = false;
                    } else if (contentRect.x > maxX2) {
                        contentRect.x += (maxX2 - contentRect.x) * springFactor;
                        scroll.velocity.x = 0.f;
                        contentRect.worldMatrixDirty = true;
                        contentRect.worldRectCached = false;
                    }
                }

                if (scroll.vertical) {
                    if (contentRect.y < minY) {
                        contentRect.y += (minY - contentRect.y) * springFactor;
                        scroll.velocity.y = 0.f;
                        contentRect.worldMatrixDirty = true;
                        contentRect.worldRectCached = false;
                    } else if (contentRect.y > maxY2) {
                        contentRect.y += (maxY2 - contentRect.y) * springFactor;
                        scroll.velocity.y = 0.f;
                        contentRect.worldMatrixDirty = true;
                        contentRect.worldRectCached = false;
                    }
                }
            }

            // Update normalized position
            float maxScrollX = std::max(0.001f, scroll.contentWidth - scroll.viewportWidth);
            float maxScrollY = std::max(0.001f, scroll.contentHeight - scroll.viewportHeight);
            float halfVpW = scroll.viewportWidth * 0.5f;
            float halfVpH = scroll.viewportHeight * 0.5f;
            float baseX = halfVpW - scroll.contentWidth * contentRect.pivotX;
            float baseY = halfVpH - scroll.contentHeight * contentRect.pivotY;
            scroll.normalizedPosition.x = std::max(0.f, std::min(1.f, (baseX - contentRect.x) / maxScrollX));
            scroll.normalizedPosition.y = std::max(0.f, std::min(1.f, (baseY - contentRect.y) / maxScrollY));
        }
    }

    //=========================================================================
    // Focus Management
    //=========================================================================

    void UIEventSystem::SetFocusedEntity(Entity e) {
        if (e == m_focusedEntity) return;

        // Blur old
        if (m_focusedEntity != NO_ENTITY) {
            if (m_cm->HasComponent<UIInputField>(m_focusedEntity)) {
                auto& field = m_cm->GetComponent<UIInputField>(m_focusedEntity);
                field.isFocused = false;
                field.selectionStart = -1;
                field.selectionEnd = -1;
            }
            NANOEngine::Events::EventBus::Get().Dispatch(
                NANOEngine::Events::EventDomain::Engine,
                NANOEngine::Events::UIBlurEvent{ m_focusedEntity }
            );
        }

        m_focusedEntity = e;

        // Focus new
        if (m_focusedEntity != NO_ENTITY) {
            if (m_cm->HasComponent<UIInputField>(m_focusedEntity)) {
                auto& field = m_cm->GetComponent<UIInputField>(m_focusedEntity);
                field.isFocused = true;
                field.cursorBlinkTimer = 0.0f;
                field.cursorVisible = true;
                // Place cursor at end of text
                field.cursorPosition = static_cast<int>(field.text.size());
                field.previousText = field.text;
            }
            NANOEngine::Events::EventBus::Get().Dispatch(
                NANOEngine::Events::EventDomain::Engine,
                NANOEngine::Events::UIFocusEvent{ m_focusedEntity }
            );
        }
    }

    void UIEventSystem::ClearFocus() {
        SetFocusedEntity(NO_ENTITY);
    }

    void UIEventSystem::HandleFocusChange(Entity clickedEntity, bool mousePressed) {
        if (!mousePressed) return;

        // Determine if clicked entity is focusable (has UIInputField)
        bool clickedIsFocusable = (clickedEntity != NO_ENTITY &&
            m_cm->HasComponent<UIInputField>(clickedEntity) &&
            m_cm->GetComponent<UIInputField>(clickedEntity).interactable);

        if (clickedIsFocusable) {
            SetFocusedEntity(clickedEntity);
        } else if (clickedEntity != m_focusedEntity) {
            // Check if clicked entity is a child of the focused entity
            bool isChildOfFocused = false;
            if (m_focusedEntity != NO_ENTITY && clickedEntity != NO_ENTITY) {
                Entity current = clickedEntity;
                while (current != NO_ENTITY) {
                    if (current == m_focusedEntity) {
                        isChildOfFocused = true;
                        break;
                    }
                    if (!m_cm->HasComponent<Hierarchy>(current)) break;
                    current = m_cm->GetComponent<Hierarchy>(current).parent;
                }
            }

            // Only blur if clicked entity is not a child of the focused input field
            if (!isChildOfFocused) {
                ClearFocus();
            }
        }
    }

    Entity UIEventSystem::FindNextFocusable(Entity current, bool reverse) {
        const auto& allEntities = GetEntities();
        std::vector<Entity> focusable;

        for (Entity e : allEntities) {
            if (!m_cm->HasComponent<UIInputField>(e)) continue;
            if (!m_cm->GetComponent<UIInputField>(e).interactable) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;

            Entity canvas = FindOwningCanvas(e);
            if (canvas != NO_ENTITY && !IsActiveForUI(e, canvas)) continue;

            focusable.push_back(e);
        }

        if (focusable.empty()) return NO_ENTITY;

        // Find current index
        auto it = std::find(focusable.begin(), focusable.end(), current);
        if (it == focusable.end()) {
            return focusable.front();
        }

        size_t idx = std::distance(focusable.begin(), it);
        if (reverse) {
            return (idx == 0) ? focusable.back() : focusable[idx - 1];
        } else {
            return (idx + 1 >= focusable.size()) ? focusable.front() : focusable[idx + 1];
        }
    }

    //=========================================================================
    // Input Field Handling
    //=========================================================================

    bool UIEventSystem::IsCharAllowed(char32_t codepoint, const UIInputField& field) {
        switch (field.contentType) {
        case UIInputField::ContentType::INTEGER:
            return (codepoint >= '0' && codepoint <= '9') || codepoint == '-';
        case UIInputField::ContentType::DECIMAL:
            return (codepoint >= '0' && codepoint <= '9') || codepoint == '-' || codepoint == '.';
        case UIInputField::ContentType::ALPHA_NUMERIC:
            return (codepoint >= 'a' && codepoint <= 'z') ||
                   (codepoint >= 'A' && codepoint <= 'Z') ||
                   (codepoint >= '0' && codepoint <= '9') ||
                   codepoint == '_';
        case UIInputField::ContentType::STANDARD:
        case UIInputField::ContentType::PASSWORD:
        default:
            return codepoint >= 32; // All printable characters
        }
    }

    void UIEventSystem::DeleteSelection(UIInputField& field) {
        if (field.selectionStart < 0 || field.selectionEnd < 0) return;

        int start = std::min(field.selectionStart, field.selectionEnd);
        int end = std::max(field.selectionStart, field.selectionEnd);
        start = std::max(0, std::min(start, static_cast<int>(field.text.size())));
        end = std::max(0, std::min(end, static_cast<int>(field.text.size())));

        field.text.erase(start, end - start);
        field.cursorPosition = start;
        field.selectionStart = -1;
        field.selectionEnd = -1;
    }

    void UIEventSystem::InsertText(UIInputField& field, const std::string& text) {
        // Delete selection first if any
        if (field.selectionStart >= 0 && field.selectionEnd >= 0 &&
            field.selectionStart != field.selectionEnd) {
            DeleteSelection(field);
        }

        // Check character limit
        if (field.characterLimit > 0 &&
            static_cast<int>(field.text.size() + text.size()) > field.characterLimit) {
            int remaining = field.characterLimit - static_cast<int>(field.text.size());
            if (remaining <= 0) return;
            field.text.insert(field.cursorPosition, text.substr(0, remaining));
            field.cursorPosition += remaining;
        } else {
            field.text.insert(field.cursorPosition, text);
            field.cursorPosition += static_cast<int>(text.size());
        }
    }

    void UIEventSystem::ProcessInputFieldKeyboard(Entity entity, UIInputField& field, double deltaTime) {
        bool ctrlHeld = NE::InputManager::IsKeyDown(GLFW_KEY_LEFT_CONTROL) ||
                        NE::InputManager::IsKeyDown(GLFW_KEY_RIGHT_CONTROL);
        bool shiftHeld = NE::InputManager::IsKeyDown(GLFW_KEY_LEFT_SHIFT) ||
                         NE::InputManager::IsKeyDown(GLFW_KEY_RIGHT_SHIFT);

        bool textChanged = false;

        // Tab — cycle focus
        if (NE::InputManager::WasKeyPressed(GLFW_KEY_TAB)) {
            Entity next = FindNextFocusable(entity, shiftHeld);
            if (next != NO_ENTITY) {
                SetFocusedEntity(next);
            }
            return;
        }

        // Escape — blur
        if (NE::InputManager::WasKeyPressed(GLFW_KEY_ESCAPE)) {
            ClearFocus();
            return;
        }

        // Enter / Return
        if (NE::InputManager::WasKeyPressed(GLFW_KEY_ENTER) ||
            NE::InputManager::WasKeyPressed(GLFW_KEY_KP_ENTER)) {
            if (field.lineType == UIInputField::LineType::SINGLE_LINE) {
                // Submit
                NANOEngine::Events::EventBus::Get().Dispatch(
                    NANOEngine::Events::EventDomain::Engine,
                    NANOEngine::Events::UIInputFieldSubmitEvent{ entity, field.text, field.onSubmitEventId }
                );
                ClearFocus();
                return;
            } else {
                // Multi-line: insert newline
                if (!field.readOnly) {
                    InsertText(field, "\n");
                    textChanged = true;
                }
            }
        }

        if (!field.readOnly) {
            // Ctrl+A — select all
            if (ctrlHeld && NE::InputManager::WasKeyPressed(GLFW_KEY_A)) {
                field.selectionStart = 0;
                field.selectionEnd = static_cast<int>(field.text.size());
                field.cursorPosition = field.selectionEnd;
            }

            // Ctrl+C — copy
            if (ctrlHeld && NE::InputManager::WasKeyPressed(GLFW_KEY_C)) {
                if (field.selectionStart >= 0 && field.selectionEnd >= 0 &&
                    field.selectionStart != field.selectionEnd &&
                    field.contentType != UIInputField::ContentType::PASSWORD) {
                    int start = std::min(field.selectionStart, field.selectionEnd);
                    int end = std::max(field.selectionStart, field.selectionEnd);
                    std::string selected = field.text.substr(start, end - start);
                    GLFWwindow* win = static_cast<GLFWwindow*>(NE::GetNativeWindowHandle());
                    if (win) glfwSetClipboardString(win, selected.c_str());
                }
            }

            // Ctrl+X — cut
            if (ctrlHeld && NE::InputManager::WasKeyPressed(GLFW_KEY_X)) {
                if (field.selectionStart >= 0 && field.selectionEnd >= 0 &&
                    field.selectionStart != field.selectionEnd &&
                    field.contentType != UIInputField::ContentType::PASSWORD) {
                    int start = std::min(field.selectionStart, field.selectionEnd);
                    int end = std::max(field.selectionStart, field.selectionEnd);
                    std::string selected = field.text.substr(start, end - start);
                    GLFWwindow* win = static_cast<GLFWwindow*>(NE::GetNativeWindowHandle());
                    if (win) glfwSetClipboardString(win, selected.c_str());
                    DeleteSelection(field);
                    textChanged = true;
                }
            }

            // Ctrl+V — paste
            if (ctrlHeld && NE::InputManager::WasKeyPressed(GLFW_KEY_V)) {
                GLFWwindow* win = static_cast<GLFWwindow*>(NE::GetNativeWindowHandle());
                if (win) {
                    const char* cb = glfwGetClipboardString(win);
                    if (cb) {
                        std::string pasteText(cb);
                        // Filter single-line: remove newlines
                        if (field.lineType == UIInputField::LineType::SINGLE_LINE) {
                            pasteText.erase(std::remove(pasteText.begin(), pasteText.end(), '\n'), pasteText.end());
                            pasteText.erase(std::remove(pasteText.begin(), pasteText.end(), '\r'), pasteText.end());
                        }
                        // Filter pasted text through content type validation
                        std::string filteredText;
                        filteredText.reserve(pasteText.size());
                        for (unsigned char ch : pasteText) {
                            if (IsCharAllowed(static_cast<char32_t>(ch), field))
                                filteredText += static_cast<char>(ch);
                        }
                        if (!filteredText.empty()) {
                            InsertText(field, filteredText);
                            textChanged = true;
                        }
                    }
                }
            }

            // Backspace
            if (NE::InputManager::WasKeyPressed(GLFW_KEY_BACKSPACE)) {
                if (field.selectionStart >= 0 && field.selectionEnd >= 0 &&
                    field.selectionStart != field.selectionEnd) {
                    DeleteSelection(field);
                    textChanged = true;
                } else if (field.cursorPosition > 0) {
                    field.text.erase(field.cursorPosition - 1, 1);
                    field.cursorPosition--;
                    textChanged = true;
                }
            }

            // Delete
            if (NE::InputManager::WasKeyPressed(GLFW_KEY_DELETE)) {
                if (field.selectionStart >= 0 && field.selectionEnd >= 0 &&
                    field.selectionStart != field.selectionEnd) {
                    DeleteSelection(field);
                    textChanged = true;
                } else if (field.cursorPosition < static_cast<int>(field.text.size())) {
                    field.text.erase(field.cursorPosition, 1);
                    textChanged = true;
                }
            }

            // Character input
            uint32_t codepoint;
            while ((codepoint = NE::InputManager::PopChar()) != 0) {
                if (!ctrlHeld && IsCharAllowed(codepoint, field)) {
                    // Convert codepoint to UTF-8 (full Unicode support)
                    // Characters not in the font atlas are dropped gracefully at render time
                    std::string utf8Text;
                    if (codepoint < 0x80) {
                        utf8Text = std::string(1, static_cast<char>(codepoint));
                    } else if (codepoint < 0x800) {
                        utf8Text += static_cast<char>(0xC0 | (codepoint >> 6));
                        utf8Text += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint < 0x10000) {
                        utf8Text += static_cast<char>(0xE0 | (codepoint >> 12));
                        utf8Text += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        utf8Text += static_cast<char>(0x80 | (codepoint & 0x3F));
                    } else if (codepoint <= 0x10FFFF) {
                        utf8Text += static_cast<char>(0xF0 | (codepoint >> 18));
                        utf8Text += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                        utf8Text += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                        utf8Text += static_cast<char>(0x80 | (codepoint & 0x3F));
                    }

                    if (!utf8Text.empty()) {
                        InsertText(field, utf8Text);
                        textChanged = true;
                    }
                }
            }
        }

        // Cursor movement (works even in readOnly)
        if (NE::InputManager::WasKeyPressed(GLFW_KEY_LEFT)) {
            if (shiftHeld) {
                if (field.selectionStart < 0) {
                    field.selectionStart = field.cursorPosition;
                    field.selectionEnd = field.cursorPosition;
                }
                if (field.cursorPosition > 0) field.cursorPosition--;
                field.selectionEnd = field.cursorPosition;
            } else {
                if (field.selectionStart >= 0 && field.selectionStart != field.selectionEnd) {
                    field.cursorPosition = std::min(field.selectionStart, field.selectionEnd);
                } else if (field.cursorPosition > 0) {
                    field.cursorPosition--;
                }
                field.selectionStart = -1;
                field.selectionEnd = -1;
            }
            field.cursorBlinkTimer = 0.0f;
            field.cursorVisible = true;
        }

        if (NE::InputManager::WasKeyPressed(GLFW_KEY_RIGHT)) {
            if (shiftHeld) {
                if (field.selectionStart < 0) {
                    field.selectionStart = field.cursorPosition;
                    field.selectionEnd = field.cursorPosition;
                }
                if (field.cursorPosition < static_cast<int>(field.text.size())) field.cursorPosition++;
                field.selectionEnd = field.cursorPosition;
            } else {
                if (field.selectionStart >= 0 && field.selectionStart != field.selectionEnd) {
                    field.cursorPosition = std::max(field.selectionStart, field.selectionEnd);
                } else if (field.cursorPosition < static_cast<int>(field.text.size())) {
                    field.cursorPosition++;
                }
                field.selectionStart = -1;
                field.selectionEnd = -1;
            }
            field.cursorBlinkTimer = 0.0f;
            field.cursorVisible = true;
        }

        if (NE::InputManager::WasKeyPressed(GLFW_KEY_HOME)) {
            if (shiftHeld) {
                if (field.selectionStart < 0) field.selectionStart = field.cursorPosition;
                field.cursorPosition = 0;
                field.selectionEnd = 0;
            } else {
                field.cursorPosition = 0;
                field.selectionStart = -1;
                field.selectionEnd = -1;
            }
        }

        if (NE::InputManager::WasKeyPressed(GLFW_KEY_END)) {
            if (shiftHeld) {
                if (field.selectionStart < 0) field.selectionStart = field.cursorPosition;
                field.cursorPosition = static_cast<int>(field.text.size());
                field.selectionEnd = field.cursorPosition;
            } else {
                field.cursorPosition = static_cast<int>(field.text.size());
                field.selectionStart = -1;
                field.selectionEnd = -1;
            }
        }

        // Clamp cursor
        field.cursorPosition = std::max(0, std::min(field.cursorPosition, static_cast<int>(field.text.size())));

        // Cursor blink
        field.cursorBlinkTimer += static_cast<float>(deltaTime);
        if (field.cursorBlinkTimer >= field.cursorBlinkRate) {
            field.cursorBlinkTimer -= field.cursorBlinkRate;
            field.cursorVisible = !field.cursorVisible;
        }

        // Reset blink on text change
        if (textChanged) {
            field.cursorBlinkTimer = 0.0f;
            field.cursorVisible = true;
        }

        // Dispatch change event
        if (textChanged && field.text != field.previousText) {
            NANOEngine::Events::EventBus::Get().Dispatch(
                NANOEngine::Events::EventDomain::Engine,
                NANOEngine::Events::UIInputFieldChangedEvent{
                    entity, field.text, field.previousText, field.onValueChangedEventId
                }
            );
            field.previousText = field.text;
        }
    }

    void UIEventSystem::ApplyInputFieldColorToImage(Entity entity) {
        if (!m_cm->HasComponent<UIInputField>(entity)) return;
        if (!m_cm->HasComponent<UIImage>(entity)) return;

        auto& field = m_cm->GetComponent<UIInputField>(entity);
        auto& image = m_cm->GetComponent<UIImage>(entity);

        if (!field.interactable) {
            image.color = field.disabledColor;
        } else if (field.isFocused) {
            image.color = field.selectedColor;
        } else {
            image.color = field.normalColor;
        }
    }

    void UIEventSystem::SyncInputFieldToText(Entity entity) {
        if (!m_cm->HasComponent<UIInputField>(entity)) return;

        auto& field = m_cm->GetComponent<UIInputField>(entity);

        // Find sibling UIText — look on the same entity first, then children
        Entity textEntity = NO_ENTITY;
        if (m_cm->HasComponent<UIText>(entity)) {
            textEntity = entity;
        } else if (m_cm->HasComponent<Hierarchy>(entity)) {
            auto& hierarchy = m_cm->GetComponent<Hierarchy>(entity);
            for (uint32_t child : hierarchy.children) {
                if (m_cm->HasComponent<UIText>(child)) {
                    textEntity = child;
                    break;
                }
            }
        }

        if (textEntity == NO_ENTITY) return;

        auto& text = m_cm->GetComponent<UIText>(textEntity);

        // Display text content or placeholder
        bool showPlaceholder = field.text.empty() && !field.isFocused;
        std::string displayText;

        if (showPlaceholder) {
            displayText = field.placeholderText;
            text.color = field.placeholderColor;
        } else if (field.contentType == UIInputField::ContentType::PASSWORD) {
            displayText = std::string(field.text.size(), field.passwordChar);
            text.color = field.textColor;
        } else {
            displayText = field.text;
            text.color = field.textColor;
        }

        if (text.text != displayText) {
            text.text = displayText;
            text.isDirty = true;
        }
    }

    void UIEventSystem::UpdateInputFields(double deltaTime) {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIInputField>(e)) continue;

            auto& field = m_cm->GetComponent<UIInputField>(e);

            // Process keyboard input for focused field
            if (field.isFocused && e == m_focusedEntity) {
                ProcessInputFieldKeyboard(e, field, deltaTime);
            }

            // Apply visual state
            ApplyInputFieldColorToImage(e);

            // Sync text display
            SyncInputFieldToText(e);
        }
    }

    //=========================================================================
    // Drag Events
    //=========================================================================

    void UIEventSystem::UpdateDragEvents(float mouseX, float mouseY, bool mouseDown, bool mousePressed, bool mouseReleased) {
        if (mousePressed && m_hoveredEntity != NO_ENTITY) {
            m_isDragging = true;
            m_lastDragX = mouseX;
            m_lastDragY = mouseY;
        }

        if (mouseReleased) {
            m_isDragging = false;
        }

        if (m_isDragging && mouseDown && m_pressedEntity != NO_ENTITY) {
            float dx = mouseX - m_lastDragX;
            float dy = mouseY - m_lastDragY;

            if (std::abs(dx) > 0.1f || std::abs(dy) > 0.1f) {
                NANOEngine::Events::EventBus::Get().Dispatch(
                    NANOEngine::Events::EventDomain::Engine,
                    NANOEngine::Events::UIPointerDragEvent{
                        m_pressedEntity, dx, dy, mouseX, mouseY
                    }
                );
                m_lastDragX = mouseX;
                m_lastDragY = mouseY;
            }
        }
    }

    //=========================================================================
    // Dropdown Handling
    //=========================================================================

    bool UIEventSystem::IsEntityInDropdownPanel(Entity entity, Entity dropdownEntity) {
        if (!m_cm->HasComponent<UIDropdown>(dropdownEntity)) return false;
        auto& dropdown = m_cm->GetComponent<UIDropdown>(dropdownEntity);
        Entity panelEntity = dropdown.optionsPanelEntity;
        if (panelEntity == UINT32_MAX) return false;

        // Check if entity IS the panel or a descendant of it
        Entity cur = entity;
        while (cur != NO_ENTITY) {
            if (cur == panelEntity) return true;
            if (!m_cm->HasComponent<Hierarchy>(cur)) break;
            cur = m_cm->GetComponent<Hierarchy>(cur).parent;
            if (cur == UINT32_MAX) break;
        }
        return false;
    }

    int UIEventSystem::GetOptionIndexFromEntity(Entity clickedEntity, Entity dropdownEntity) {
        if (!m_cm->HasComponent<UIDropdown>(dropdownEntity)) return -1;
        auto& dropdown = m_cm->GetComponent<UIDropdown>(dropdownEntity);
        Entity panelEntity = dropdown.optionsPanelEntity;
        if (panelEntity == UINT32_MAX) return -1;
        if (!m_cm->HasComponent<Hierarchy>(panelEntity)) return -1;

        auto& panelHierarchy = m_cm->GetComponent<Hierarchy>(panelEntity);
        for (size_t i = 0; i < panelHierarchy.children.size(); ++i) {
            Entity child = panelHierarchy.children[i];
            // Clicked directly on the option entity or one of its children (UIText)
            if (clickedEntity == child) return static_cast<int>(i);
            if (m_cm->HasComponent<Hierarchy>(clickedEntity)) {
                Entity parent = m_cm->GetComponent<Hierarchy>(clickedEntity).parent;
                if (parent == child) return static_cast<int>(i);
            }
        }
        return -1;
    }

    void UIEventSystem::ExpandDropdown(Entity dropdownEntity) {
        // Collapse any currently expanded dropdown first
        if (m_expandedDropdown != NO_ENTITY && m_expandedDropdown != dropdownEntity) {
            CollapseDropdown();
        }

        if (!m_cm->HasComponent<UIDropdown>(dropdownEntity)) return;
        auto& dropdown = m_cm->GetComponent<UIDropdown>(dropdownEntity);
        Entity panelEntity = dropdown.optionsPanelEntity;
        if (panelEntity == UINT32_MAX) return;

        // Activate the panel entity
        //if (m_cm->HasComponent<EntityMeta>(panelEntity)) {
        //    m_cm->GetComponent<EntityMeta>(panelEntity).isActive = true;
        //}
        m_em->ToggleActive(panelEntity, true); // Might need to rewire to ECSCoordinator as well ~Irwen

        dropdown.isExpanded = true;
        dropdown.hoveredOptionIndex = -1;
        m_expandedDropdown = dropdownEntity;

        // Sync option text to panel children
        SyncDropdownOptionsToPanel(dropdownEntity);
    }

    void UIEventSystem::CollapseDropdown() {
        if (m_expandedDropdown == NO_ENTITY) return;
        if (!m_cm->HasComponent<UIDropdown>(m_expandedDropdown)) {
            m_expandedDropdown = NO_ENTITY;
            return;
        }

        auto& dropdown = m_cm->GetComponent<UIDropdown>(m_expandedDropdown);
        Entity panelEntity = dropdown.optionsPanelEntity;

        // Deactivate the panel entity
        //if (panelEntity != UINT32_MAX && m_cm->HasComponent<EntityMeta>(panelEntity)) {
        //    m_cm->GetComponent<EntityMeta>(panelEntity).isActive = false;
        //}
        if (panelEntity != ECS::NO_ENTITY) m_em->ToggleActive(panelEntity, false);

        dropdown.isExpanded = false;
        dropdown.hoveredOptionIndex = -1;
        m_expandedDropdown = NO_ENTITY;
    }

    void UIEventSystem::SyncDropdownOptionsToPanel(Entity dropdownEntity) {
        if (!m_cm->HasComponent<UIDropdown>(dropdownEntity)) return;
        auto& dropdown = m_cm->GetComponent<UIDropdown>(dropdownEntity);
        Entity panelEntity = dropdown.optionsPanelEntity;
        if (panelEntity == UINT32_MAX) return;
        if (!m_cm->HasComponent<Hierarchy>(panelEntity)) return;

        auto& panelHierarchy = m_cm->GetComponent<Hierarchy>(panelEntity);

        for (size_t i = 0; i < panelHierarchy.children.size(); ++i) {
            Entity child = panelHierarchy.children[i];

            // Hide children that exceed option count
            if (i >= dropdown.options.size()) {
                //if (m_cm->HasComponent<EntityMeta>(child)) {
                //    m_cm->GetComponent<EntityMeta>(child).isActive = false;
                //}
                m_em->ToggleActive(child, false);
                continue;
            }

            // Show and set text for valid options
            //if (m_cm->HasComponent<EntityMeta>(child)) {
            //    m_cm->GetComponent<EntityMeta>(child).isActive = true;
            //}
            m_em->ToggleActive(child, true);

            // Set text — check child directly or its first UIText child
            if (m_cm->HasComponent<UIText>(child)) {
                m_cm->GetComponent<UIText>(child).text = dropdown.options[i];
            } else if (m_cm->HasComponent<Hierarchy>(child)) {
                for (uint32_t grandchild : m_cm->GetComponent<Hierarchy>(child).children) {
                    if (m_cm->HasComponent<UIText>(grandchild)) {
                        m_cm->GetComponent<UIText>(grandchild).text = dropdown.options[i];
                        break;
                    }
                }
            }

            // Apply normal color to option image
            if (m_cm->HasComponent<UIImage>(child)) {
                m_cm->GetComponent<UIImage>(child).color = dropdown.optionNormalColor;
            }
        }
    }

    void UIEventSystem::SyncDropdownCaptionText(Entity dropdownEntity) {
        if (!m_cm->HasComponent<UIDropdown>(dropdownEntity)) return;
        auto& dropdown = m_cm->GetComponent<UIDropdown>(dropdownEntity);

        Entity captionEntity = dropdown.captionTextEntity;
        if (captionEntity == UINT32_MAX) return;
        if (!m_cm->HasComponent<UIText>(captionEntity)) return;

        auto& text = m_cm->GetComponent<UIText>(captionEntity);
        if (dropdown.selectedIndex >= 0 && dropdown.selectedIndex < static_cast<int>(dropdown.options.size())) {
            text.text = dropdown.options[dropdown.selectedIndex];
        } else {
            text.text = "";
        }
    }

    void UIEventSystem::ApplyDropdownColorToImage(Entity dropdownEntity) {
        if (!m_cm->HasComponent<UIDropdown>(dropdownEntity)) return;
        if (!m_cm->HasComponent<UIImage>(dropdownEntity)) return;

        auto& dropdown = m_cm->GetComponent<UIDropdown>(dropdownEntity);
        auto& image = m_cm->GetComponent<UIImage>(dropdownEntity);

        if (!dropdown.interactable) {
            image.color = dropdown.disabledColor;
        } else if (m_pressedEntity == dropdownEntity) {
            image.color = dropdown.pressedColor;
        } else if (m_hoveredEntity == dropdownEntity || dropdown.isExpanded) {
            image.color = dropdown.highlightedColor;
        } else {
            image.color = dropdown.normalColor;
        }
    }

    void UIEventSystem::UpdateDropdowns(float mouseX, float mouseY, bool mousePressed, bool mouseReleased) {
        const auto& entities = GetEntities();

        // Handle clicks
        if (mousePressed) {
            bool clickedOnDropdownOrPanel = false;

            // Check if we clicked on a dropdown entity
            if (m_hoveredEntity != NO_ENTITY && m_cm->HasComponent<UIDropdown>(m_hoveredEntity)) {
                auto& dropdown = m_cm->GetComponent<UIDropdown>(m_hoveredEntity);
                if (dropdown.interactable) {
                    if (dropdown.isExpanded) {
                        CollapseDropdown();
                    } else {
                        ExpandDropdown(m_hoveredEntity);
                    }
                    clickedOnDropdownOrPanel = true;
                }
            }

            // Check if we clicked on an option in the expanded panel
            if (!clickedOnDropdownOrPanel && m_expandedDropdown != NO_ENTITY && m_hoveredEntity != NO_ENTITY) {
                if (IsEntityInDropdownPanel(m_hoveredEntity, m_expandedDropdown)) {
                    int optionIndex = GetOptionIndexFromEntity(m_hoveredEntity, m_expandedDropdown);
                    if (optionIndex >= 0) {
                        auto& dropdown = m_cm->GetComponent<UIDropdown>(m_expandedDropdown);
                        int prevIndex = dropdown.selectedIndex;
                        dropdown.selectedIndex = optionIndex;
                        dropdown.previousSelectedIndex = prevIndex;

                        // Sync caption text
                        SyncDropdownCaptionText(m_expandedDropdown);

                        // Dispatch event
                        std::string selectedOption;
                        if (optionIndex < static_cast<int>(dropdown.options.size())) {
                            selectedOption = dropdown.options[optionIndex];
                        }
                        NANOEngine::Events::EventBus::Get().Dispatch(
                            NANOEngine::Events::EventDomain::Engine,
                            NANOEngine::Events::UIDropdownValueChangedEvent{
                                m_expandedDropdown,
                                optionIndex,
                                prevIndex,
                                selectedOption,
                                dropdown.onValueChangedEventId
                            }
                        );

                        CollapseDropdown();
                    }
                    clickedOnDropdownOrPanel = true;
                }
            }

            // Click outside expanded dropdown — collapse
            if (!clickedOnDropdownOrPanel && m_expandedDropdown != NO_ENTITY) {
                CollapseDropdown();
            }
        }

        // Update hover highlighting on option panel children
        if (m_expandedDropdown != NO_ENTITY && m_cm->HasComponent<UIDropdown>(m_expandedDropdown)) {
            auto& dropdown = m_cm->GetComponent<UIDropdown>(m_expandedDropdown);
            Entity panelEntity = dropdown.optionsPanelEntity;

            if (panelEntity != UINT32_MAX && m_cm->HasComponent<Hierarchy>(panelEntity)) {
                auto& panelHierarchy = m_cm->GetComponent<Hierarchy>(panelEntity);
                int newHovered = -1;

                // Determine which option is hovered
                if (m_hoveredEntity != NO_ENTITY) {
                    newHovered = GetOptionIndexFromEntity(m_hoveredEntity, m_expandedDropdown);
                }

                dropdown.hoveredOptionIndex = newHovered;

                // Apply highlight colors to option images
                for (size_t i = 0; i < panelHierarchy.children.size() && i < dropdown.options.size(); ++i) {
                    Entity child = panelHierarchy.children[i];
                    if (!m_cm->HasComponent<UIImage>(child)) continue;
                    auto& img = m_cm->GetComponent<UIImage>(child);

                    if (static_cast<int>(i) == newHovered) {
                        img.color = dropdown.optionHighlightedColor;
                    } else {
                        img.color = dropdown.optionNormalColor;
                    }
                }
            }
        }

        // Update all dropdown states (color, caption sync)
        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIDropdown>(e)) continue;

            Entity canvasEntity = FindOwningCanvas(e);
            if (canvasEntity != NO_ENTITY && !IsActiveForUI(e, canvasEntity)) continue;

            ApplyDropdownColorToImage(e);
            SyncDropdownCaptionText(e);
        }
    }

} // namespace NE::ECS::Systems