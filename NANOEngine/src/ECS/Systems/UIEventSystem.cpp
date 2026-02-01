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
        // Clear wasClicked flags at the start of each frame
        const auto& entities = GetEntities();
        for (Entity e : entities) {
            if (m_cm->HasComponent<UIButton>(e)) {
                auto& button = m_cm->GetComponent<UIButton>(e);
                button.wasClicked = false;
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
            }
            m_pressedEntity = NO_ENTITY;
        }

        m_hoveredEntity = hoveredEntity;

        // Update button states
        UpdateButtonStates();
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

        // For each canvas, find button entities
        for (const auto& [canvasEntity, canvasPtr] : canvases) {
            for (Entity e : allEntities) {
                if (e == canvasEntity) continue;
                if (!m_cm->HasComponent<UIButton>(e)) continue;
                if (!m_cm->HasComponent<UIRectTransform>(e)) continue;

                auto& button = m_cm->GetComponent<UIButton>(e);
                if (!button.interactable) continue;

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
