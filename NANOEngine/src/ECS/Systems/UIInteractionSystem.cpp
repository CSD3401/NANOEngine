#include "UIInteractionSystem.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/NativeScript.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Core/Logger.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Math/Vec4.hpp"
#include "../../Math/Mat4.hpp"
#include "../../Events/EventBus.hpp"
#include "../../Events/UIButtonEvents.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <limits>

namespace NE::ECS::Systems {

    float UIInteractionSystem::s_viewportX = 0.0f;
    float UIInteractionSystem::s_viewportY = 0.0f;
    float UIInteractionSystem::s_viewportWidth = 0.0f;
    float UIInteractionSystem::s_viewportHeight = 0.0f;
    bool UIInteractionSystem::s_viewportSet = false;
    
    NE::Math::Mat4 UIInteractionSystem::s_cameraView;
    NE::Math::Mat4 UIInteractionSystem::s_cameraProjection;
    bool UIInteractionSystem::s_cameraMatricesSet = false;

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
    
    void UIInteractionSystem::SetCameraMatrices(const NE::Math::Mat4& view, const NE::Math::Mat4& projection) {
        s_cameraView = view;
        s_cameraProjection = projection;
        s_cameraMatricesSet = true;
    }

    void UIInteractionSystem::Init() {
        m_wasPressedLastFrame.clear();
    }

    void UIInteractionSystem::Update(double deltaTime) {
        if (!m_transformSystem) return;

        auto [mouseX, mouseY] = NE::InputManager::MousePos();
        bool isMouseDown = NE::InputManager::IsMouseDown(0);
        bool wasMousePressed = NE::InputManager::WasMousePressed(0);
        bool wasMouseReleased = NE::InputManager::WasMouseReleased(0);

        const auto& canvasEntities = m_cm->GetEntitiesWithComponent<Component::UICanvas>();

        for (Entity canvasEntity : canvasEntities) {
            if (!m_cm->HasComponent<Component::UICanvas>(canvasEntity)) continue;

            auto& canvas = m_cm->GetComponent<Component::UICanvas>(canvasEntity);

            if (canvas.renderMode == Component::UICanvas::RenderMode::SCREEN_SPACE_OVERLAY) {
                ProcessScreenSpaceButtons(canvasEntity);
            }
            else if (canvas.renderMode == Component::UICanvas::RenderMode::WORLD_SPACE) {
                ProcessWorldSpaceButtons(canvasEntity);
            }
        }
        
        ProcessButtonClicks();
    }

    void UIInteractionSystem::Exit() {
        m_wasPressedLastFrame.clear();
    }

    void UIInteractionSystem::OnEntityAdded(Entity e) {
        m_entities.Insert(e);
        
        if (m_cm->HasComponent<Component::UIButton>(e)) {
            auto& button = m_cm->GetComponent<Component::UIButton>(e);
            
            if (button.interactable) {
                button.currentState = Component::UIButton::State::NORMAL;
            } else {
                button.currentState = Component::UIButton::State::DISABLED;
            }
            
            m_wasPressedLastFrame[e] = false;
        }
    }

    void UIInteractionSystem::OnEntityRemoved(Entity e) {
        m_entities.Remove(e);
        m_wasPressedLastFrame.erase(e);
    }

    void UIInteractionSystem::SetTransformSystem(UITransformSystem* transformSystem) {
        m_transformSystem = transformSystem;
    }

    Entity UIInteractionSystem::FindCanvasForEntity(Entity entity) {
        Entity current = entity;
        int maxDepth = 100;
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
        // TODO: Add rotation support later if needed
        
        float pivotX = worldTransform.x;
        float pivotY = worldTransform.y;
        float width = worldTransform.width;
        float height = worldTransform.height;

        float pivotXNorm = rect.pivotX;
        float pivotYNorm = rect.pivotY;

        // Calculate top-left corner from pivot
        float topLeftX = pivotX - width * pivotXNorm;
        float topLeftY = pivotY - height * (1.0f - pivotYNorm);

        float left = topLeftX;
        float right = topLeftX + width;
        float top = topLeftY;
        float bottom = topLeftY + height;

        float mouseXf = static_cast<float>(mouseX);
        float mouseYf = static_cast<float>(mouseY);

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

        if (!button.interactable) {
            button.currentState = Component::UIButton::State::DISABLED;
            return;
        }

        Component::UIButton::State previousState = button.currentState;

        if (isPressed && isHovering) {
            button.currentState = Component::UIButton::State::PRESSED;
            if (previousState != Component::UIButton::State::PRESSED) {
                m_wasPressedLastFrame[entity] = true;
            }
        } else if (isHovering) {
            button.currentState = Component::UIButton::State::HOVERED;
        } else {
            button.currentState = Component::UIButton::State::NORMAL;
        }
    }

    void UIInteractionSystem::ProcessScreenSpaceButtons(Entity canvasEntity) {
        if (!m_transformSystem) return;

        auto& canvas = m_cm->GetComponent<Component::UICanvas>(canvasEntity);

        auto [mouseX, mouseY] = NE::InputManager::MousePos();
        bool isMouseDown = NE::InputManager::IsMouseDown(0);

        uint32_t screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        uint32_t screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
        
        uint32_t windowWidth = NE::Graphics::GraphicsManager::GetWindowWidth();
        uint32_t windowHeight = NE::Graphics::GraphicsManager::GetWindowHeight();

        float mouseXScreen = static_cast<float>(mouseX);
        float mouseYScreen = static_cast<float>(mouseY);
        
        // Convert mouse coordinates from window/viewport space to screen space
        if (s_viewportSet && s_viewportWidth > 0.0f && s_viewportHeight > 0.0f) {
            if (mouseX >= s_viewportX && mouseX < s_viewportX + s_viewportWidth &&
                mouseY >= s_viewportY && mouseY < s_viewportY + s_viewportHeight) {
                float normalizedX = (mouseX - s_viewportX) / s_viewportWidth;
                float normalizedY = (mouseY - s_viewportY) / s_viewportHeight;
                
                // Account for letterboxing when aspect ratios differ
                float panelAspect = s_viewportWidth / s_viewportHeight;
                float screenAspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
                
                if (std::abs(panelAspect - screenAspect) > 0.01f) {
                    float scaleX = 1.0f;
                    float scaleY = 1.0f;
                    float offsetX = 0.0f;
                    float offsetY = 0.0f;
                    
                    if (panelAspect > screenAspect) {
                        scaleY = 1.0f;
                        scaleX = screenAspect / panelAspect;
                        offsetX = (1.0f - scaleX) * 0.5f;
                    } else {
                        scaleX = 1.0f;
                        scaleY = panelAspect / screenAspect;
                        offsetY = (1.0f - scaleY) * 0.5f;
                    }
                    
                    normalizedX = (normalizedX - offsetX) / scaleX;
                    normalizedY = (normalizedY - offsetY) / scaleY;
                    
                    if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedY < 0.0f || normalizedY > 1.0f) {
                        return;
                    }
                }
                
                mouseXScreen = normalizedX * static_cast<float>(screenWidth);
                mouseYScreen = normalizedY * static_cast<float>(screenHeight);
            } else {
                return;
            }
        } else {
            if (windowWidth > 0 && windowHeight > 0 && 
                screenWidth > 0 && screenHeight > 0 &&
                (windowWidth != screenWidth || windowHeight != screenHeight)) {
                mouseXScreen = (mouseXScreen / static_cast<float>(windowWidth)) * static_cast<float>(screenWidth);
                mouseYScreen = (mouseYScreen / static_cast<float>(windowHeight)) * static_cast<float>(screenHeight);
            }
        }

        std::vector<Entity> buttonEntities;
        const auto& allButtonEntities = m_cm->GetEntitiesWithComponent<Component::UIButton>();

        for (Entity entity : allButtonEntities) {
            Entity buttonCanvas = FindCanvasForEntity(entity);
            if (buttonCanvas == canvasEntity) {
                buttonEntities.push_back(entity);
            }
        }

        // Sort by Z-order (higher Z = processed first)
        std::sort(buttonEntities.begin(), buttonEntities.end(),
            [this](Entity a, Entity b) {
                if (!m_cm->HasComponent<Component::UIRectTransform>(a) ||
                    !m_cm->HasComponent<Component::UIRectTransform>(b)) {
                    return false;
                }
                auto& rectA = m_cm->GetComponent<Component::UIRectTransform>(a);
                auto& rectB = m_cm->GetComponent<Component::UIRectTransform>(b);
                return rectA.z > rectB.z;
            });

        Entity hoveredButton = NE::ECS::NO_ENTITY;
        Entity pressedButton = NE::ECS::NO_ENTITY;
        
        for (Entity entity : buttonEntities) {
            if (!m_cm->HasComponent<Component::UIRectTransform>(entity)) continue;
            if (!m_cm->HasComponent<Component::UIButton>(entity)) continue;

            UITransformSystem::WorldTransform worldTransform = 
                m_transformSystem->CalculateWorldTransform(entity, canvasEntity, canvas);

            auto& rect = m_cm->GetComponent<Component::UIRectTransform>(entity);
            float rotationZ = worldTransform.accumulatedRotationZ;
            
            bool isHovering = IsPointInRect(mouseXScreen, mouseYScreen, worldTransform, rect, rotationZ);
            bool isPressing = isHovering && isMouseDown;

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
        for (Entity entity : buttonEntities) {
            bool isHovering = (entity == hoveredButton);
            bool isPressed = (entity == pressedButton) && isMouseDown;
            UpdateButtonState(entity, isHovering, isPressed);
        }
    }

    UIInteractionSystem::Ray UIInteractionSystem::ScreenToRay(
        double mouseX, double mouseY,
        const NE::Math::Mat4& viewMatrix,
        const NE::Math::Mat4& projMatrix,
        float viewportX, float viewportY,
        float viewportWidth, float viewportHeight)
    {
        Ray ray;
        
        // Convert mouse coordinates to normalized device coordinates (NDC)
        // NDC: X and Y range from -1 to 1, with (0,0) at center
        float ndcX = ((mouseX - viewportX) / viewportWidth) * 2.0f - 1.0f;
        float ndcY = 1.0f - ((mouseY - viewportY) / viewportHeight) * 2.0f; // Flip Y (screen Y-down to NDC Y-up)
        
        // Create two points in clip space (near and far planes)
        NE::Math::Vec4 nearPoint(ndcX, ndcY, -1.0f, 1.0f); // Near plane in NDC
        NE::Math::Vec4 farPoint(ndcX, ndcY, 1.0f, 1.0f);   // Far plane in NDC
        
        // Get inverse view-projection matrix
        NE::Math::Mat4 viewProj = projMatrix * viewMatrix;
        NE::Math::Mat4 invViewProj = viewProj.Inverse();
        
        // Transform to world space (manual matrix-vector multiplication)
        auto Mat4MulVec4 = [](const NE::Math::Mat4& m, const NE::Math::Vec4& v) -> NE::Math::Vec4 {
            return NE::Math::Vec4(
                m.GetElement(0, 0) * v.x + m.GetElement(0, 1) * v.y + m.GetElement(0, 2) * v.z + m.GetElement(0, 3) * v.w,
                m.GetElement(1, 0) * v.x + m.GetElement(1, 1) * v.y + m.GetElement(1, 2) * v.z + m.GetElement(1, 3) * v.w,
                m.GetElement(2, 0) * v.x + m.GetElement(2, 1) * v.y + m.GetElement(2, 2) * v.z + m.GetElement(2, 3) * v.w,
                m.GetElement(3, 0) * v.x + m.GetElement(3, 1) * v.y + m.GetElement(3, 2) * v.z + m.GetElement(3, 3) * v.w
            );
        };
        
        NE::Math::Vec4 nearWorld = Mat4MulVec4(invViewProj, nearPoint);
        NE::Math::Vec4 farWorld = Mat4MulVec4(invViewProj, farPoint);
        
        // Perspective divide
        if (std::abs(nearWorld.w) > 1e-5f) {
            nearWorld.x /= nearWorld.w;
            nearWorld.y /= nearWorld.w;
            nearWorld.z /= nearWorld.w;
        }
        if (std::abs(farWorld.w) > 1e-5f) {
            farWorld.x /= farWorld.w;
            farWorld.y /= farWorld.w;
            farWorld.z /= farWorld.w;
        }
        
        // Ray origin is the near point, direction is from near to far
        ray.origin = NE::Math::Vec3(nearWorld.x, nearWorld.y, nearWorld.z);
        NE::Math::Vec3 farPos(farWorld.x, farWorld.y, farWorld.z);
        ray.direction = (farPos - ray.origin).Normalized();
        
        return ray;
    }
    
    bool UIInteractionSystem::RayIntersectsUIElement(
        const Ray& ray,
        const UITransformSystem::WorldTransform& worldTransform,
        const Component::UIRectTransform& rect,
        const UITransformSystem::AccumulatedTransform& accumulated,
        NE::Math::Vec3& outIntersectionPoint)
    {
        if (!m_transformSystem) return false;
        
        Entity entity = NE::ECS::NO_ENTITY;
        const auto& entities = m_cm->GetEntitiesWithComponent<Component::UIRectTransform>();
        for (Entity e : entities) {
            if (&m_cm->GetComponent<Component::UIRectTransform>(e) == &rect) {
                entity = e;
                break;
            }
        }
        if (entity == NE::ECS::NO_ENTITY) return false;
        
        Entity canvasEntity = FindCanvasForEntity(entity);
        if (canvasEntity == NE::ECS::NO_ENTITY) return false;
        
        NE::Math::Mat4 modelMatrix = m_transformSystem->BuildWorldSpaceModelMatrix(
            entity,
            canvasEntity,
            rect,
            accumulated,
            1.0f
        );
        
        NE::Math::Vec3 localNormal(0.0f, 0.0f, 1.0f);
        NE::Math::Vec3 planeNormal(
            modelMatrix.GetElement(0, 0) * localNormal.x + modelMatrix.GetElement(0, 1) * localNormal.y + modelMatrix.GetElement(0, 2) * localNormal.z,
            modelMatrix.GetElement(1, 0) * localNormal.x + modelMatrix.GetElement(1, 1) * localNormal.y + modelMatrix.GetElement(1, 2) * localNormal.z,
            modelMatrix.GetElement(2, 0) * localNormal.x + modelMatrix.GetElement(2, 1) * localNormal.y + modelMatrix.GetElement(2, 2) * localNormal.z
        );
        planeNormal.Normalize();
        
        NE::Math::Vec3 pivotPos(
            modelMatrix.GetElement(0, 3),
            modelMatrix.GetElement(1, 3),
            modelMatrix.GetElement(2, 3)
        );
        
        float planeD = planeNormal.Dot(pivotPos);
        float denominator = planeNormal.Dot(ray.direction);
        
        if (std::abs(denominator) < 1e-5f) {
            return false;
        }
        
        float numerator = planeD - planeNormal.Dot(ray.origin);
        float t = numerator / denominator;
        
        if (t < 0.0f) {
            return false;
        }
        
        outIntersectionPoint = ray.origin + ray.direction * t;
        
        NE::Math::Mat4 invModelMatrix = modelMatrix.Inverse();
        NE::Math::Vec4 worldPoint(outIntersectionPoint.x, outIntersectionPoint.y, outIntersectionPoint.z, 1.0f);
        
        NE::Math::Vec4 localPoint(
            invModelMatrix.GetElement(0, 0) * worldPoint.x + invModelMatrix.GetElement(0, 1) * worldPoint.y + invModelMatrix.GetElement(0, 2) * worldPoint.z + invModelMatrix.GetElement(0, 3) * worldPoint.w,
            invModelMatrix.GetElement(1, 0) * worldPoint.x + invModelMatrix.GetElement(1, 1) * worldPoint.y + invModelMatrix.GetElement(1, 2) * worldPoint.z + invModelMatrix.GetElement(1, 3) * worldPoint.w,
            invModelMatrix.GetElement(2, 0) * worldPoint.x + invModelMatrix.GetElement(2, 1) * worldPoint.y + invModelMatrix.GetElement(2, 2) * worldPoint.z + invModelMatrix.GetElement(2, 3) * worldPoint.w,
            invModelMatrix.GetElement(3, 0) * worldPoint.x + invModelMatrix.GetElement(3, 1) * worldPoint.y + invModelMatrix.GetElement(3, 2) * worldPoint.z + invModelMatrix.GetElement(3, 3) * worldPoint.w
        );
        
        if (std::abs(localPoint.w) > 1e-5f) {
            localPoint.x /= localPoint.w;
            localPoint.y /= localPoint.w;
            localPoint.z /= localPoint.w;
        }
        
        if (localPoint.x >= 0.0f && localPoint.x <= 1.0f &&
            localPoint.y >= 0.0f && localPoint.y <= 1.0f) {
            return true;
        }
        
        return false;
    }
    
    void UIInteractionSystem::ProcessWorldSpaceButtons(Entity canvasEntity) {
        if (!m_transformSystem) return;
        if (!s_cameraMatricesSet) return;
        
        auto& canvas = m_cm->GetComponent<Component::UICanvas>(canvasEntity);
        
        auto [mouseX, mouseY] = NE::InputManager::MousePos();
        bool isMouseDown = NE::InputManager::IsMouseDown(0);
        
        float viewportX = s_viewportSet ? s_viewportX : 0.0f;
        float viewportY = s_viewportSet ? s_viewportY : 0.0f;
        float viewportWidth = s_viewportSet ? s_viewportWidth : static_cast<float>(NE::Graphics::GraphicsManager::GetWindowWidth());
        float viewportHeight = s_viewportSet ? s_viewportHeight : static_cast<float>(NE::Graphics::GraphicsManager::GetWindowHeight());
        
        Ray ray = ScreenToRay(mouseX, mouseY, s_cameraView, s_cameraProjection, 
                             viewportX, viewportY, viewportWidth, viewportHeight);
        
        // Collect all child entities of this canvas that have UIButton
        std::vector<Entity> buttonEntities;
        const auto& allButtonEntities = m_cm->GetEntitiesWithComponent<Component::UIButton>();
        
        for (Entity entity : allButtonEntities) {
            Entity buttonCanvas = FindCanvasForEntity(entity);
            if (buttonCanvas == canvasEntity) {
                buttonEntities.push_back(entity);
            }
        }
        
        std::sort(buttonEntities.begin(), buttonEntities.end(),
            [this](Entity a, Entity b) {
                if (!m_cm->HasComponent<Component::UIRectTransform>(a) ||
                    !m_cm->HasComponent<Component::UIRectTransform>(b)) {
                    return false;
                }
                auto& rectA = m_cm->GetComponent<Component::UIRectTransform>(a);
                auto& rectB = m_cm->GetComponent<Component::UIRectTransform>(b);
                return rectA.z > rectB.z;
            });
        
        Entity hoveredButton = NE::ECS::NO_ENTITY;
        Entity pressedButton = NE::ECS::NO_ENTITY;
        float closestIntersection = std::numeric_limits<float>::max();
        
        for (Entity entity : buttonEntities) {
            if (!m_cm->HasComponent<Component::UIRectTransform>(entity)) continue;
            if (!m_cm->HasComponent<Component::UIButton>(entity)) continue;
            
            UITransformSystem::WorldTransform worldTransform = 
                m_transformSystem->CalculateWorldTransform(entity, canvasEntity, canvas);
            
            UITransformSystem::AccumulatedTransform accumulated = 
                m_transformSystem->AccumulateParentTransforms(entity, canvasEntity, canvas);
            
            auto& rect = m_cm->GetComponent<Component::UIRectTransform>(entity);
            
            NE::Math::Vec3 intersectionPoint;
            if (RayIntersectsUIElement(ray, worldTransform, rect, accumulated, intersectionPoint)) {
                float distance = (intersectionPoint - ray.origin).Length();
                
                if (distance < closestIntersection) {
                    closestIntersection = distance;
                    hoveredButton = entity;
                    if (isMouseDown) {
                        pressedButton = entity;
                    }
                }
            }
        }
        for (Entity entity : buttonEntities) {
            bool isHovering = (entity == hoveredButton);
            bool isPressed = (entity == pressedButton) && isMouseDown;
            
            UpdateButtonState(entity, isHovering, isPressed);
        }
    }

    void UIInteractionSystem::ProcessButtonClicks() {
        const auto& buttonEntities = m_cm->GetEntitiesWithComponent<Component::UIButton>();
        
        for (Entity entity : buttonEntities) {
            if (m_cm->HasComponent<Component::UIButton>(entity)) {
                auto& button = m_cm->GetComponent<Component::UIButton>(entity);
                button.wasClickedThisFrame = false;
            }
        }
        
        bool isMouseDown = NE::InputManager::IsMouseDown(0);
        bool wasMouseReleased = NE::InputManager::WasMouseReleased(0);
        
        if (!wasMouseReleased) return;
        for (Entity entity : buttonEntities) {
            if (!m_cm->HasComponent<Component::UIButton>(entity)) continue;
            
            auto& button = m_cm->GetComponent<Component::UIButton>(entity);
            
            if (!button.interactable) continue;
            
            bool wasPressed = m_wasPressedLastFrame[entity];
            bool isCurrentlyHovering = (button.currentState == Component::UIButton::State::HOVERED || 
                                       button.currentState == Component::UIButton::State::NORMAL);
            
            if (wasPressed && wasMouseReleased && isCurrentlyHovering) {
                button.wasClickedThisFrame = true;
                m_wasPressedLastFrame[entity] = false;
                
                NANOEngine::Events::UIButtonClickEvent clickEvent{ entity, button.onClickEventId };
                
                NANOEngine::Events::EventBus::Get().Dispatch(
                    NANOEngine::Events::EventDomain::Engine,
                    clickEvent
                );
                
                NANOEngine::Events::EventBus::Get().Dispatch(
                    NANOEngine::Events::EventDomain::Script,
                    clickEvent
                );
            }
        }
    }

} // namespace NE::ECS::Systems

