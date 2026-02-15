#include "UIEventSystem.hpp"
#include "../../Input/InputManager.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/EditorCamera.hpp"
#include "../Components/Transform.hpp"
#include "../../Math/Vec4.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include "../Components/EntityMeta.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/Hierarchy.hpp"
#include "../../Events/EventBus.hpp"
#include "../../Events/UIEvents.hpp"
#include "UITransformUtilities.hpp"

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

    UIEventSystem::UIEventSystem(ComponentManager* cm) : m_cm(cm) {}

    void UIEventSystem::Init() {}
    Entity UIEventSystem::FindOwningCanvas(Entity entity) const
    {
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
        return UIUtil::IsActiveForUI(m_cm, entity, canvasEntity);
    }

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

        // Find topmost screen-space element under mouse
        Entity hoveredEntity = NO_ENTITY;
        for (const auto& elem : elements) {
            if (PointInRect(static_cast<float>(mouseX), static_cast<float>(mouseY), elem)) {
                hoveredEntity = elem.entity;
                break;
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
                bool metaActive = true;
                if (m_cm->HasComponent<NE::ECS::Component::EntityMeta>(e)) {
                    metaActive = m_cm->GetComponent<NE::ECS::Component::EntityMeta>(e).isActive;
                }

                if (canvas.isActive && metaActive &&
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
            Entity canvasEntity = FindOwningCanvas(e);
            if (canvasEntity != NO_ENTITY) {
                if (!IsActiveForUI(e, canvasEntity)) continue;
            }
            else {
                // If it's not on a canvas, still respect its EntityMeta
                if (m_cm->HasComponent<NE::ECS::Component::EntityMeta>(e) &&
                    !m_cm->GetComponent<NE::ECS::Component::EntityMeta>(e).isActive) {
                    continue;
                }
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

        // Handle slider drag
        if (mouseDown && m_draggingSlider != NO_ENTITY) {
            if (m_cm->HasComponent<UISlider>(m_draggingSlider) &&
                m_cm->HasComponent<UIRectTransform>(m_draggingSlider)) {

                auto& slider = m_cm->GetComponent<UISlider>(m_draggingSlider);
                auto& rect = m_cm->GetComponent<UIRectTransform>(m_draggingSlider);

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
                        NANOEngine::Events::EventBus::Get().Dispatch(
                            NANOEngine::Events::EventDomain::Engine,
                            NANOEngine::Events::UISliderValueChangedEvent{ m_draggingSlider, slider.value, oldValue }
                        );
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

            accScaleX *= r.scaleX;
            accScaleY *= r.scaleY;
            accScaleZ *= r.scaleZ;
            accRotX += r.rotationX;
            accRotY += r.rotationY;
            accRotZ += r.rotationZ;
            accPosX += r.x;
            accPosY += r.y;
            accPosZ += r.z;
        }

        // Compute pivot offset
        Math::Vec2 pivot = rect.GetPivot();
        float pivotOffsetX = -rect.width * pivot.x * accScaleX;
        float pivotOffsetY = -rect.height * pivot.y * accScaleY;

        // Build matrices
        Math::Mat4 scaleMatrix = Math::Mat4::BuildScaling(
            rect.width * accScaleX,
            rect.height * accScaleY,
            accScaleZ
        );

        Math::Mat4 pivotMatrix = Math::Mat4::BuildTranslation(pivotOffsetX, pivotOffsetY, 0.0f);

        Math::Mat4 rotationX = Math::Mat4::BuildXRotation(accRotX * PI / 180.0f);
        Math::Mat4 rotationY = Math::Mat4::BuildYRotation(accRotY * PI / 180.0f);
        Math::Mat4 rotationZ = Math::Mat4::BuildZRotation(accRotZ * PI / 180.0f);
        Math::Mat4 rotationMatrix = rotationZ * rotationY * rotationX;

        Math::Mat4 translationMatrix = Math::Mat4::BuildTranslation(accPosX, accPosY, accPosZ);

        return translationMatrix * rotationMatrix * pivotMatrix * scaleMatrix;
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
                if (canvas.isActive && canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                    worldCanvases.push_back({ e, &canvas });
                }
            }
        }

        // For each world-space canvas, collect interactable elements
        for (const auto& [canvasEntity, canvasPtr] : worldCanvases) {
            for (Entity e : allEntities) {
                if (e == canvasEntity) continue;
                if (!m_cm->HasComponent<UIRectTransform>(e)) continue;

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

} // namespace NE::ECS::Systems
