#include "UIInteractionSystem.hpp"
#include "../Components/UIImage.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Core/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace NE::ECS::Systems {

    // Static viewport bounds (set by editor)
    float UIInteractionSystem::s_viewportX = 0.0f;
    float UIInteractionSystem::s_viewportY = 0.0f;
    float UIInteractionSystem::s_viewportWidth = 0.0f;
    float UIInteractionSystem::s_viewportHeight = 0.0f;
    bool UIInteractionSystem::s_viewportSet = false;

    UIInteractionSystem::UIInteractionSystem(ComponentManager* cm, UITransformSystem* transformSystem)
        : m_cm(cm), m_transformSystem(transformSystem)
    {
    }
    
    void UIInteractionSystem::SetViewportBounds(float x, float y, float width, float height) {
        s_viewportX = x;
        s_viewportY = y;
        s_viewportWidth = width;
        s_viewportHeight = height;
        s_viewportSet = (width > 0.0f && height > 0.0f);
    }

    void UIInteractionSystem::Init() {
        m_wasPressedLastFrame.clear();
    }

    void UIInteractionSystem::Update(double deltaTime) {
        if (!m_transformSystem) return;

        // Get mouse position and button state
        auto [mouseX, mouseY] = NE::InputManager::MousePos();
        bool isMouseDown = NE::InputManager::IsMouseDown(0); // Left mouse button
        bool wasMousePressed = NE::InputManager::WasMousePressed(0);
        bool wasMouseReleased = NE::InputManager::WasMouseReleased(0);

        // Find all canvases
        const auto& canvasEntities = m_cm->GetEntitiesWithComponent<Component::UICanvas>();

        for (Entity canvasEntity : canvasEntities) {
            if (!m_cm->HasComponent<Component::UICanvas>(canvasEntity)) continue;

            auto& canvas = m_cm->GetComponent<Component::UICanvas>(canvasEntity);

            // Only process Screen Space Overlay for now
            if (canvas.renderMode == Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY) {
                ProcessScreenSpaceButtons(canvasEntity);
            }
            // TODO: Process world space buttons later
            // else if (canvas.renderMode == Component::UICanvas::RenderMode::WORLD_SPACE) {
            //     ProcessWorldSpaceButtons(canvasEntity);
            // }
        }

        // Update pressed state tracking for next frame
        const auto& buttonEntities = m_cm->GetEntitiesWithComponent<Component::UIButton>();
        for (Entity entity : buttonEntities) {
            if (m_cm->HasComponent<Component::UIButton>(entity)) {
                auto& button = m_cm->GetComponent<Component::UIButton>(entity);
                m_wasPressedLastFrame[entity] = (button.currentState == Component::UIButton::State::PRESSED);
            }
        }
    }

    void UIInteractionSystem::Exit() {
        m_wasPressedLastFrame.clear();
    }

    void UIInteractionSystem::OnEntityAdded(Entity e) {
        m_entities.Insert(e);
    }

    void UIInteractionSystem::OnEntityRemoved(Entity e) {
        m_entities.Remove(e);
        m_wasPressedLastFrame.erase(e);
    }

    void UIInteractionSystem::SetTransformSystem(UITransformSystem* transformSystem) {
        m_transformSystem = transformSystem;
    }

    Entity UIInteractionSystem::FindCanvasForEntity(Entity entity) {
        // Walk up the parent chain to find the canvas
        Entity current = entity;
        int maxDepth = 100; // Safety limit
        int depth = 0;

        while (current != NE::ECS::NO_ENTITY && depth < maxDepth) {
            if (m_cm->HasComponent<Component::UICanvas>(current)) {
                return current;
            }

            if (m_cm->HasComponent<Component::UIRectTransform>(current)) {
                auto& rect = m_cm->GetComponent<Component::UIRectTransform>(current);
                current = rect.parent;
            } else {
                break;
            }

            depth++;
        }

        return NE::ECS::NO_ENTITY;
    }

    bool UIInteractionSystem::IsPointInRect(
        double mouseX,
        double mouseY,
        const UITransformSystem::WorldTransform& worldTransform,
        const Component::UIRectTransform& rect,
        float rotationDegrees
    ) {
        // For now, handle simple axis-aligned rectangles (rotation = 0)
        // TODO: Add rotation support later if needed

        // Calculate rectangle bounds in screen space (top-left origin, Y-down)
        // worldTransform.x/y is the pivot position in screen pixels
        // We need to calculate the top-left corner from the pivot
        // This matches the calculation in UIRenderSystem::GenerateScreenSpaceVertices
        
        float pivotX = worldTransform.x;
        float pivotY = worldTransform.y;
        float width = worldTransform.width;
        float height = worldTransform.height;

        // Get the pivot values from the rect transform
        float pivotXNorm = rect.pivotX;
        float pivotYNorm = rect.pivotY;

        // Calculate top-left corner from pivot (same as UIRenderSystem)
        // Formula: topLeft = pivot - (width * pivotX, height * (1 - pivotY))
        // In Y-down: pivotY=0.0 is bottom (larger Y), pivotY=1.0 is top (smaller Y)
        float topLeftX = pivotX - width * pivotXNorm;
        float topLeftY = pivotY - height * (1.0f - pivotYNorm);

        // Calculate bounds
        float left = topLeftX;
        float right = topLeftX + width;
        float top = topLeftY;  // Y-down: top is smaller Y
        float bottom = topLeftY + height; // Y-down: bottom is larger Y

        // Convert mouse coordinates to float for comparison
        float mouseXf = static_cast<float>(mouseX);
        float mouseYf = static_cast<float>(mouseY);

        // DEBUG: Log coordinates when mouse is pressed (only once per press to avoid spam)
        static bool s_wasMouseDown = false;
        static bool s_loggedThisPress = false;
        bool isMouseDownNow = NE::InputManager::IsMouseDown(0);
        
        if (isMouseDownNow && !s_wasMouseDown) {
            // Mouse just pressed - log coordinates
            s_loggedThisPress = false;
        }
        
        if (isMouseDownNow && !s_loggedThisPress) {
            std::cout << "\n=== UI INTERACTION DEBUG ===" << std::endl;
            std::cout << "Mouse (window space): (" << mouseX << ", " << mouseY << ")" << std::endl;
            std::cout << "Mouse (screen space): (" << std::fixed << std::setprecision(2) << mouseXf << ", " << mouseYf << ")" << std::endl;
            std::cout << "World Transform:" << std::endl;
            std::cout << "  Pivot: (" << pivotX << ", " << pivotY << ")" << std::endl;
            std::cout << "  Size: " << width << " x " << height << std::endl;
            std::cout << "  Pivot Norm: (" << pivotXNorm << ", " << pivotYNorm << ")" << std::endl;
            std::cout << "Button Bounds:" << std::endl;
            std::cout << "  Top-Left: (" << topLeftX << ", " << topLeftY << ")" << std::endl;
            std::cout << "  Left: " << left << ", Right: " << right << std::endl;
            std::cout << "  Top: " << top << ", Bottom: " << bottom << std::endl;
            std::cout << "  Center: (" << (left + right) / 2.0f << ", " << (top + bottom) / 2.0f << ")" << std::endl;
            
            bool inBoundsX = (mouseXf >= (left - 0.5f) && mouseXf <= (right + 0.5f));
            bool inBoundsY = (mouseYf >= (top - 0.5f) && mouseYf <= (bottom + 0.5f));
            std::cout << "Point Check:" << std::endl;
            std::cout << "  X in bounds: " << (inBoundsX ? "YES" : "NO") 
                      << " (mouseX=" << mouseXf << ", left=" << left << ", right=" << right << ")" << std::endl;
            std::cout << "  Y in bounds: " << (inBoundsY ? "YES" : "NO") 
                      << " (mouseY=" << mouseYf << ", top=" << top << ", bottom=" << bottom << ")" << std::endl;
            std::cout << "  Distance from center: (" << (mouseXf - (left + right) / 2.0f) 
                      << ", " << (mouseYf - (top + bottom) / 2.0f) << ")" << std::endl;
            std::cout << "  Result: " << (inBoundsX && inBoundsY ? "INSIDE" : "OUTSIDE") << std::endl;
            std::cout << "============================\n" << std::endl;
            
            s_loggedThisPress = true;
        }
        
        s_wasMouseDown = isMouseDownNow;

        // Check if mouse is within bounds
        // Mouse coordinates from InputManager are in window/screen pixels (top-left origin, Y-down)
        // World transform coordinates are in screen pixels (already scaled by scaleFactor)
        // For screen space overlay, they should match directly
        // Add small tolerance for floating point precision
        const float tolerance = 0.5f;
        return (mouseXf >= (left - tolerance) && mouseXf <= (right + tolerance) && 
                mouseYf >= (top - tolerance) && mouseYf <= (bottom + tolerance));
    }

    void UIInteractionSystem::UpdateButtonState(
        Entity entity,
        bool isHovering,
        bool isPressed
    ) {
        if (!m_cm->HasComponent<Component::UIButton>(entity)) return;

        auto& button = m_cm->GetComponent<Component::UIButton>(entity);

        // If button is disabled, don't process interactions
        if (!button.interactable) {
            button.currentState = Component::UIButton::State::DISABLED;
            return;
        }

        // State machine for button interactions
        // NORMAL -> HOVERED (mouse enters)
        // HOVERED -> PRESSED (mouse down)
        // PRESSED -> HOVERED (mouse up, still hovering)
        // PRESSED -> NORMAL (mouse up, not hovering)
        // HOVERED -> NORMAL (mouse leaves)

        if (isPressed && isHovering) {
            button.currentState = Component::UIButton::State::PRESSED;
        } else if (isHovering) {
            button.currentState = Component::UIButton::State::HOVERED;
        } else {
            button.currentState = Component::UIButton::State::NORMAL;
        }
    }

    void UIInteractionSystem::ProcessScreenSpaceButtons(Entity canvasEntity) {
        if (!m_transformSystem) return;

        // Get canvas component first to access scaleFactor
        auto& canvas = m_cm->GetComponent<Component::UICanvas>(canvasEntity);

        // Get mouse position (in window/screen pixels from GLFW)
        auto [mouseX, mouseY] = NE::InputManager::MousePos();
        bool isMouseDown = NE::InputManager::IsMouseDown(0);

        // Get screen dimensions (UI is rendered at this resolution)
        uint32_t screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        uint32_t screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
        
        // Get window dimensions (mouse coordinates are in this space)
        uint32_t windowWidth = NE::Graphics::GraphicsManager::GetWindowWidth();
        uint32_t windowHeight = NE::Graphics::GraphicsManager::GetWindowHeight();

        // Convert mouse coordinates from window space to screen space
        // UI world transform coordinates are in screen pixels (top-left origin, Y-down)
        // Mouse coordinates from GLFW are in window pixels (top-left origin, Y-down)
        // 
        // For screen space overlay:
        // - UI is rendered at screen resolution (e.g., 1920x1080)
        // - Mouse coordinates are in window pixels
        // - If viewport is set (UI rendered in a panel), convert panel-relative to screen space
        // - Otherwise, scale window coordinates to screen space
        float mouseXScreen = static_cast<float>(mouseX);
        float mouseYScreen = static_cast<float>(mouseY);
        
        // Always use viewport bounds if set (UI is rendered in Scene panel, not full window)
        if (s_viewportSet && s_viewportWidth > 0.0f && s_viewportHeight > 0.0f) {
            // Viewport bounds are in window coordinates (matching GLFW mouse coordinates)
            // Mouse coordinates from GLFW are also in window coordinates
            // Check if mouse is within viewport bounds
            if (mouseX >= s_viewportX && mouseX < s_viewportX + s_viewportWidth &&
                mouseY >= s_viewportY && mouseY < s_viewportY + s_viewportHeight) {
                // Convert panel-relative coordinates to normalized (0-1) within the viewport
                float normalizedX = (mouseX - s_viewportX) / s_viewportWidth;
                float normalizedY = (mouseY - s_viewportY) / s_viewportHeight;
                
                // Convert normalized to screen space
                // The UI is rendered at screen resolution (1920x1080) and composited into the scene framebuffer
                // The scene framebuffer is then displayed in the panel using ImGui::Image() with panelSize
                // ImGui::Image() scales the texture to fit the panel, maintaining aspect ratio (letterboxing if needed)
                // So we need to account for the aspect ratio difference between panel and screen
                
                // Calculate aspect ratios
                float panelAspect = s_viewportWidth / s_viewportHeight;
                float screenAspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
                
                // If aspect ratios differ, the UI is letterboxed (black bars on sides or top/bottom)
                if (std::abs(panelAspect - screenAspect) > 0.01f) {
                    // Letterboxing: calculate the actual UI display area within the panel
                    float scaleX = 1.0f;
                    float scaleY = 1.0f;
                    float offsetX = 0.0f;
                    float offsetY = 0.0f;
                    
                    if (panelAspect > screenAspect) {
                        // Panel is wider: letterboxing on sides
                        scaleY = 1.0f;
                        scaleX = screenAspect / panelAspect;
                        offsetX = (1.0f - scaleX) * 0.5f;
                    } else {
                        // Panel is taller: letterboxing on top/bottom
                        scaleX = 1.0f;
                        scaleY = panelAspect / screenAspect;
                        offsetY = (1.0f - scaleY) * 0.5f;
                    }
                    
                    // Adjust normalized coordinates to account for letterboxing
                    normalizedX = (normalizedX - offsetX) / scaleX;
                    normalizedY = (normalizedY - offsetY) / scaleY;
                    
                    // Clamp to valid range (in case click was in letterbox area)
                    if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedY < 0.0f || normalizedY > 1.0f) {
                        return; // Click was in letterbox area, not on UI
                    }
                }
                
                // Convert normalized coordinates to screen space
                mouseXScreen = normalizedX * static_cast<float>(screenWidth);
                mouseYScreen = normalizedY * static_cast<float>(screenHeight);
            } else {
                // Mouse is outside viewport, don't process interactions
                return;
            }
        } else {
            // No viewport set: use direct window-to-screen scaling
            // This handles the case where UI fills the entire window (shouldn't happen in editor)
            if (windowWidth > 0 && windowHeight > 0 && 
                screenWidth > 0 && screenHeight > 0 &&
                (windowWidth != screenWidth || windowHeight != screenHeight)) {
                mouseXScreen = (mouseXScreen / static_cast<float>(windowWidth)) * static_cast<float>(screenWidth);
                mouseYScreen = (mouseYScreen / static_cast<float>(windowHeight)) * static_cast<float>(screenHeight);
            }
            // If window size == screen size, mouseXScreen and mouseYScreen are already correct (no conversion needed)
        }
        
        // DEBUG: Log coordinate conversion (only when mouse is pressed)
        static bool s_wasMouseDown = false;
        static bool s_loggedConversion = false;
        if (isMouseDown && !s_wasMouseDown) {
            s_loggedConversion = false;
        }
        if (isMouseDown && !s_loggedConversion) {
            std::cout << "\n=== COORDINATE CONVERSION DEBUG ===" << std::endl;
            std::cout << "Window Size: " << windowWidth << " x " << windowHeight << std::endl;
            std::cout << "Screen Size: " << screenWidth << " x " << screenHeight << std::endl;
            std::cout << "Mouse (window): (" << mouseX << ", " << mouseY << ")" << std::endl;
            if (s_viewportSet) {
                std::cout << "Viewport: (" << s_viewportX << ", " << s_viewportY 
                          << "), Size: (" << s_viewportWidth << ", " << s_viewportHeight << ")" << std::endl;
                bool inViewport = (mouseX >= s_viewportX && mouseX < s_viewportX + s_viewportWidth &&
                                   mouseY >= s_viewportY && mouseY < s_viewportY + s_viewportHeight);
                std::cout << "Mouse in viewport: " << (inViewport ? "YES" : "NO") << std::endl;
                if (inViewport) {
                    float normX = (mouseX - s_viewportX) / s_viewportWidth;
                    float normY = (mouseY - s_viewportY) / s_viewportHeight;
                    std::cout << "Normalized: (" << normX << ", " << normY << ")" << std::endl;
                    std::cout << "Converted to screen: (" << (normX * screenWidth) << ", " << (normY * screenHeight) << ")" << std::endl;
                }
            }
            s_loggedConversion = true;
        }
        s_wasMouseDown = isMouseDown;
        
        // Note: mouseXScreen and mouseYScreen are already converted to screen space above
        // (either via viewport conversion or direct scaling)
        // No additional conversion needed here
        // If window size == screen size (fullscreen) or invalid, use mouse coordinates directly

        // Collect all child entities of this canvas that have UIButton
        std::vector<Entity> buttonEntities;

        // Get all entities with UIButton component
        const auto& allButtonEntities = m_cm->GetEntitiesWithComponent<Component::UIButton>();

        for (Entity entity : allButtonEntities) {
            // Check if this button belongs to this canvas
            Entity buttonCanvas = FindCanvasForEntity(entity);
            if (buttonCanvas == canvasEntity) {
                buttonEntities.push_back(entity);
            }
        }

        // Sort by Z-order (process back-to-front, so front elements get priority)
        // Higher Z values are rendered on top
        std::sort(buttonEntities.begin(), buttonEntities.end(),
            [this](Entity a, Entity b) {
                if (!m_cm->HasComponent<Component::UIRectTransform>(a) ||
                    !m_cm->HasComponent<Component::UIRectTransform>(b)) {
                    return false;
                }
                auto& rectA = m_cm->GetComponent<Component::UIRectTransform>(a);
                auto& rectB = m_cm->GetComponent<Component::UIRectTransform>(b);
                return rectA.z > rectB.z; // Higher Z = processed first (top layer)
            });

        // Process buttons from front to back (highest Z first)
        // First button that contains the mouse gets the interaction
        Entity hoveredButton = NE::ECS::NO_ENTITY;
        Entity pressedButton = NE::ECS::NO_ENTITY;

        // DEBUG: Log all buttons being checked
        static bool s_loggedButtons = false;
        if (isMouseDown && !s_loggedButtons) {
            std::cout << "\n=== CHECKING BUTTONS ===" << std::endl;
            std::cout << "Total buttons: " << buttonEntities.size() << std::endl;
            s_loggedButtons = true;
        }
        
        for (Entity entity : buttonEntities) {
            if (!m_cm->HasComponent<Component::UIRectTransform>(entity)) continue;
            if (!m_cm->HasComponent<Component::UIButton>(entity)) continue;

            // Get world transform for this button
            UITransformSystem::WorldTransform worldTransform = 
                m_transformSystem->CalculateWorldTransform(entity, canvasEntity, canvas);

            // Get rect transform for pivot values
            auto& rect = m_cm->GetComponent<Component::UIRectTransform>(entity);

            // Get rotation (for future rotation support)
            float rotationZ = worldTransform.accumulatedRotationZ;

            // DEBUG: Log button info
            if (isMouseDown && !s_loggedButtons) {
                float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
                float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);
                std::cout << "Button " << entity << ": pivot=(" << worldTransform.x << ", " << worldTransform.y 
                          << "), topLeft=(" << topLeftX << ", " << topLeftY << "), size=(" 
                          << worldTransform.width << ", " << worldTransform.height << ")" << std::endl;
            }

            // Check if mouse is within button bounds
            // Mouse coordinates have been converted to screen space above
            // World transform coordinates are in screen pixels (scaled by canvas.scaleFactor)
            // Both are now in the same coordinate system (screen pixels, top-left origin, Y-down)
            
            bool isHovering = IsPointInRect(mouseXScreen, mouseYScreen, worldTransform, rect, rotationZ);
            bool isPressing = isHovering && isMouseDown; // Only consider pressing if hovering

            if (isHovering) {
                if (hoveredButton == NE::ECS::NO_ENTITY) {
                    hoveredButton = entity;
                }
            }
            
            if (isPressing) {
                if (pressedButton == NE::ECS::NO_ENTITY) {
                    pressedButton = entity;
                }
            }
        }
        
        if (isMouseDown && s_loggedButtons) {
            // Reset flag when mouse is released
            static bool s_wasMouseDownLastFrame = false;
            if (!isMouseDown && s_wasMouseDownLastFrame) {
                s_loggedButtons = false;
            }
            s_wasMouseDownLastFrame = isMouseDown;
        }

        // Update all button states
        for (Entity entity : buttonEntities) {
            bool isHovering = (entity == hoveredButton);
            bool isPressed = (entity == pressedButton) && isMouseDown;

            UpdateButtonState(entity, isHovering, isPressed);
        }
    }

    void UIInteractionSystem::ProcessWorldSpaceButtons(Entity canvasEntity) {
        // TODO: Implement world space button interaction using raycast
        // This will be implemented in a future step
    }

} // namespace NE::ECS::Systems

