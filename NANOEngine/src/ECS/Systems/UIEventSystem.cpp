#include "UIEventSystem.hpp"
#include "../../Input/InputManager.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include <algorithm>
#include <cmath>

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    static constexpr float DEFAULT_ANCHOR_X = 0.5f;
    static constexpr float DEFAULT_ANCHOR_Y = 0.5f;

    UIEventSystem::UIEventSystem(ComponentManager* cm) : m_cm(cm) {}

    void UIEventSystem::Init() {}

    void UIEventSystem::OnEntityAdded(Entity e) {}

    void UIEventSystem::OnEntityRemoved(Entity e) {
        if (e == m_hoveredEntity) {
            m_hoveredEntity = NO_ENTITY;
        }
        if (e == m_pressedEntity) {
            m_pressedEntity = NO_ENTITY;
        }
    }

    void UIEventSystem::Exit() {}

    void UIEventSystem::Update(double) {
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
        }

        // Get mouse position
        auto [mouseX, mouseY] = NE::InputManager::MousePos();
        bool mouseDown = NE::InputManager::IsMouseDown(0);
        bool mousePressed = NE::InputManager::WasMousePressed(0);
        bool mouseReleased = NE::InputManager::WasMouseReleased(0);

        // Collect all interactable UI elements
        std::vector<UIElementInfo> elements = CollectInteractableElements();

        // Sort by Z-order (higher Z = on top = checked first for hits)
        std::sort(elements.begin(), elements.end(),
            [](const UIElementInfo& a, const UIElementInfo& b) {
                return a.zOrder > b.zOrder;
            });

        // Find topmost element under mouse
        Entity hoveredEntity = NO_ENTITY;
        for (const auto& elem : elements) {
            if (PointInRect(static_cast<float>(mouseX), static_cast<float>(mouseY), elem)) {
                hoveredEntity = elem.entity;
                break;
            }
        }

        // Handle press/release state
        if (mousePressed && hoveredEntity != NO_ENTITY) {
            m_pressedEntity = hoveredEntity;
        }

        if (mouseReleased) {
            // Check for click (release over same entity that was pressed)
            if (m_pressedEntity != NO_ENTITY && m_pressedEntity == hoveredEntity) {
                if (m_cm->HasComponent<UIButton>(m_pressedEntity)) {
                    auto& button = m_cm->GetComponent<UIButton>(m_pressedEntity);
                    if (button.interactable) {
                        button.wasClicked = true;
                    }
                }
                // Handle toggle click
                if (m_cm->HasComponent<UIToggle>(m_pressedEntity)) {
                    auto& toggle = m_cm->GetComponent<UIToggle>(m_pressedEntity);
                    if (toggle.interactable) {
                        toggle.wasClicked = true;
                        toggle.Toggle();
                    }
                }
                // Also check if this is a toggle background
                // Find parent toggle
                if (m_cm->HasComponent<UIRectTransform>(m_pressedEntity)) {
                    auto& rect = m_cm->GetComponent<UIRectTransform>(m_pressedEntity);
                    Entity parent = rect.parent;
                    if (parent != NO_ENTITY && m_cm->HasComponent<UIToggle>(parent)) {
                        auto& toggle = m_cm->GetComponent<UIToggle>(parent);
                        if (toggle.background == m_pressedEntity && toggle.interactable) {
                            toggle.wasClicked = true;
                            toggle.Toggle();
                        }
                    }
                }
            }
            m_pressedEntity = NO_ENTITY;
        }

        m_hoveredEntity = hoveredEntity;

        // Update button states
        UpdateButtonStates();

        // Update slider states (drag handling)
        UpdateSliderStates(static_cast<float>(mouseX), static_cast<float>(mouseY), mouseDown, mousePressed, mouseReleased);

        // Update toggle states
        UpdateToggleStates();
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
                if (canvas.isActive &&
                    (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
                     canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA)) {
                    canvases.push_back({ e, &canvas });
                }
            }
        }

        // For each canvas, find interactable entities (buttons, sliders, toggles)
        for (const auto& [canvasEntity, canvasPtr] : canvases) {
            for (Entity e : allEntities) {
                if (e == canvasEntity) continue;
                if (!m_cm->HasComponent<UIRectTransform>(e)) continue;

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
                // Check if this is a toggle background (has image with raycastTarget)
                if (m_cm->HasComponent<UIImage>(e)) {
                    auto& image = m_cm->GetComponent<UIImage>(e);
                    if (image.raycastTarget) isInteractable = true;
                }

                if (!isInteractable) continue;

                auto& rect = m_cm->GetComponent<UIRectTransform>(e);

                // Check if this entity belongs to this canvas
                Entity root = e;
                Entity current = rect.parent;
                while (current != NO_ENTITY) {
                    root = current;
                    if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                    current = m_cm->GetComponent<UIRectTransform>(current).parent;
                }

                if (root != canvasEntity && rect.parent != canvasEntity) {
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

                elements.push_back(info);
            }
        }

        return elements;
    }

    bool UIEventSystem::PointInRect(float px, float py, const UIElementInfo& element) {
        return px >= element.worldX &&
               px <= element.worldX + element.worldWidth &&
               py >= element.worldY &&
               py <= element.worldY + element.worldHeight;
    }

    void UIEventSystem::UpdateButtonStates() {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIButton>(e)) continue;

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

        // Handle slider drag
        if (mouseDown && m_draggingSlider != NO_ENTITY) {
            if (m_cm->HasComponent<UISlider>(m_draggingSlider) &&
                m_cm->HasComponent<UIRectTransform>(m_draggingSlider)) {

                auto& slider = m_cm->GetComponent<UISlider>(m_draggingSlider);
                auto& rect = m_cm->GetComponent<UIRectTransform>(m_draggingSlider);

                // Find canvas for this slider
                Entity canvasEntity = NO_ENTITY;
                Entity current = rect.parent;
                while (current != NO_ENTITY) {
                    if (m_cm->HasComponent<UICanvas>(current)) {
                        canvasEntity = current;
                        break;
                    }
                    if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                    current = m_cm->GetComponent<UIRectTransform>(current).parent;
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
                        if (worldHeight > 0.0f) {
                            normalized = (mouseY - worldY) / worldHeight;
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
                    }

                    // Update fill rect width/height based on value
                    if (slider.fillRect != UINT32_MAX && m_cm->HasComponent<UIRectTransform>(slider.fillRect)) {
                        auto& fillRect = m_cm->GetComponent<UIRectTransform>(slider.fillRect);
                        float fillNormalized = slider.GetNormalizedValue();

                        if (slider.IsHorizontal()) {
                            fillRect.width = rect.width * fillNormalized;
                        } else {
                            fillRect.height = rect.height * fillNormalized;
                        }
                    }

                    // Update handle position
                    if (slider.handleRect != UINT32_MAX && m_cm->HasComponent<UIRectTransform>(slider.handleRect)) {
                        auto& handleRect = m_cm->GetComponent<UIRectTransform>(slider.handleRect);
                        float handleNormalized = slider.GetNormalizedValue();

                        if (slider.IsHorizontal()) {
                            float trackWidth = rect.width - handleRect.width;
                            handleRect.x = trackWidth * handleNormalized - trackWidth / 2.0f;
                        } else {
                            float trackHeight = rect.height - handleRect.height;
                            handleRect.y = trackHeight * handleNormalized - trackHeight / 2.0f;
                        }
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
        float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

        switch (canvas.scaleMode) {
            case UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE: {
                float widthScale = screenWidth / canvas.referenceWidth;
                float heightScale = screenHeight / canvas.referenceHeight;
                return std::min(widthScale, heightScale);
            }
            case UICanvas::ScaleMode::CONSTANT_PIXEL_SIZE:
                return 1.0f;
            case UICanvas::ScaleMode::CONSTANT_PHYSICAL_SIZE:
                return 1.0f;
        }
        return 1.0f;
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
        float scaleFactor = CalculateScaleFactor(canvas);

        // Build parent chain (excluding canvas for screen space)
        std::vector<Entity> chain;
        Entity current = entity;
        while (current != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(current)) {
            if (current == canvasEntity) break;
            chain.push_back(current);
            current = m_cm->GetComponent<UIRectTransform>(current).parent;
        }
        std::reverse(chain.begin(), chain.end());

        // Accumulate transforms
        float accScaleX = scaleFactor;
        float accScaleY = scaleFactor;
        float accPosX = 0.0f;
        float accPosY = 0.0f;

        for (size_t i = 0; i < chain.size(); ++i) {
            Entity ent = chain[i];
            auto& r = m_cm->GetComponent<UIRectTransform>(ent);
            bool isTarget = (ent == entity);

            accScaleX *= r.scaleX;
            accScaleY *= r.scaleY;

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

} // namespace NE::ECS::Systems
