#include "UIRenderSystem.hpp"
#include "UITransformSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../Components/UIButton.hpp"
#include "../../Graphics/Core/UIDrawCommand.hpp"
#include "../../Graphics/Core/UIRenderer.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/EditorCamera.hpp"
#include "ResourceManagement/ResourceManager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    //=========================================================================
    // Constants
    //=========================================================================

    static constexpr float PI = 3.14159265358979f;
    static constexpr float ROTATION_EPSILON = 0.001f;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    UIRenderSystem::UIRenderSystem(ComponentManager* cm, UITransformSystem* transformSystem) 
        : m_cm(cm), m_transformSystem(transformSystem) {}

    void UIRenderSystem::Init() {
        const auto& entities = GetEntities();

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UIImage>(e)) continue;

            auto& img = m_cm->GetComponent<UIImage>(e);

            if (!img.textureUUID.empty() && img.bindlessHandle == 0) {
                auto texture = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
                if (texture) {
                    img.bindlessHandle = texture->GetBindlessHandle();
                }
            }

            if (!img.materialUUID.empty() && !img.material) {
                img.material = NE::Resource::ResourceManager::GetInstance()
                    .LoadResource<NE::Graphics::Material>(img.materialUUID);
            }
        }
    }

    void UIRenderSystem::OnEntityAdded(Entity e) {
        if (!m_cm->HasComponent<UIImage>(e)) return;

        auto& img = m_cm->GetComponent<UIImage>(e);

        if (!img.textureUUID.empty()) {
            auto texture = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::OpenGL::GLTexture>(img.textureUUID);
            if (texture) {
                img.bindlessHandle = texture->GetBindlessHandle();
            }
        }

        if (!img.materialUUID.empty()) {
            img.material = NE::Resource::ResourceManager::GetInstance()
                .LoadResource<NE::Graphics::Material>(img.materialUUID);
        }

        img.isDirty = true;
    }

    void UIRenderSystem::OnEntityRemoved(Entity e) {}

    void UIRenderSystem::Exit() {}

    void UIRenderSystem::SetTransformSystem(UITransformSystem* transformSystem) {
        m_transformSystem = transformSystem;
    }


    //=========================================================================
    // Rendering
    //=========================================================================

    std::vector<Entity> UIRenderSystem::CollectCanvasChildren(Entity canvasEntity) {
        const auto& entities = GetEntities();
        std::vector<Entity> canvasChildren;

        for (Entity e : entities) {
            if (e == canvasEntity) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
            if (!m_cm->HasComponent<UIImage>(e)) continue;
            if (m_cm->HasComponent<UICanvas>(e)) continue;

            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            Entity root = e;
            Entity current = rect.parent;

            while (current != NO_ENTITY) {
                root = current;
                if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                current = m_cm->GetComponent<UIRectTransform>(current).parent;
            }

            if (root == canvasEntity || rect.parent == canvasEntity) {
                canvasChildren.push_back(e);
            }
        }

        return canvasChildren;
    }

    void UIRenderSystem::SortEntitiesByZOrder(std::vector<Entity>& entities) {
        std::sort(entities.begin(), entities.end(),
            [this](Entity a, Entity b) {
                auto& rectA = m_cm->GetComponent<UIRectTransform>(a);
                auto& rectB = m_cm->GetComponent<UIRectTransform>(b);
                return rectA.z < rectB.z;
            });
    }

    std::vector<NE::Graphics::UIVertex> UIRenderSystem::GenerateScreenSpaceVertices(
        Entity entity,
        const UITransformSystem::WorldTransform& worldTransform,
        const Component::UIImage& img
    ) {
        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        // Unity-style: worldTransform is already in top-left origin coordinates (Y-down)
        // worldTransform.x/y is the pivot position (calculated in AccumulateParentTransforms)
        // GenerateVertices expects top-left corner, so we calculate it from pivot position
        //
        // In Unity: pivot (0,0) = bottom-left, (0.5,0.5) = center, (1,1) = top-right
        // Formula: topLeft = pivot - (width * pivotX, height * (1 - pivotY))
        //   - X: pivotX_norm = 0.0 → left, 1.0 → right
        //   - Y: pivotY_norm = 0.0 → bottom, 1.0 → top (in Y-down: bottom Y > top Y)
        float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
        float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);
        
        auto vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            topLeftX,
            topLeftY,
            worldTransform.z,
            worldTransform.width,
            worldTransform.height,
            img.color
        );

        if (!vertices.empty() && std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON) {
            // Rotate vertices around the pivot point
            // worldTransform.x/y is already the pivot position in top-left origin coordinates
            float pivotX = worldTransform.x;
            float pivotY = worldTransform.y;
            RotateVertices2D(vertices, pivotX, pivotY, worldTransform.accumulatedRotationZ);
        }

        return vertices;
    }

    std::vector<NE::Graphics::UIVertex> UIRenderSystem::GenerateWorldSpaceVertices(const Component::UIImage& img) {
        return NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            0.0f, 0.0f, 0.0f,
            1.0f, 1.0f,
            img.color
        );
    }


    void UIRenderSystem::SubmitDrawCommand(
        Entity entity,
        Entity canvasEntity,
        const Component::UICanvas& canvas,
        const Component::UIImage& img,
        const Component::UIRectTransform& rect,
        const UITransformSystem::WorldTransform& worldTransform,
        const UITransformSystem::AccumulatedTransform& accumulated,
        std::vector<NE::Graphics::UIVertex>& vertices,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        NE::Graphics::UIDrawCommand cmd;

        // BuildQuadVertices expects top-left corner, not pivot position
        // Calculate top-left from pivot: topLeft = pivot - (width * pivotX, height * (1 - pivotY))
        float topLeftX = worldTransform.x - worldTransform.width * rect.pivotX;
        float topLeftY = worldTransform.y - worldTransform.height * (1.0f - rect.pivotY);
        
        cmd.x = topLeftX;
        cmd.y = topLeftY;
        cmd.z = worldTransform.z;
        cmd.width = worldTransform.width;
        cmd.height = worldTransform.height;
        
        // Apply button state color if this entity has a button component
        // Unity pattern: Final Color = UIImage Color * Button State Color
        // This allows UIImage color to act as a base tint, and button states to modify it
        NE::Math::Vec4 finalColor = img.color;
        if (m_cm->HasComponent<UIButton>(entity)) {
            const auto& button = m_cm->GetComponent<UIButton>(entity);
            if (button.transitionType == UIButton::TransitionType::COLOR_TINT) {
                // Get the button color based on current state
                // GetCurrentColor() handles interactable check and returns disabledColor if not interactable
                NE::Math::Vec4 buttonColor = button.GetCurrentColor();
                
                // Multiply image color by button state color (component-wise)
                // This matches Unity's behavior: base image color is tinted by button state
                // Example: UIImage color (0.8, 0.8, 0.8) * Button hover (0.5, 1.0, 0.5) = (0.4, 0.8, 0.4)
                finalColor.x = img.color.x * buttonColor.x;
                finalColor.y = img.color.y * buttonColor.y;
                finalColor.z = img.color.z * buttonColor.z;
                finalColor.w = img.color.w * buttonColor.w;
            }
        }
        cmd.color = finalColor;
        cmd.order = canvas.sortingOrder;
        cmd.entityId = entity;
        cmd.renderMode = static_cast<int>(canvas.renderMode);
        cmd.planeDistance = canvas.planeDistance;

        cmd.material = img.material;
        cmd.bindlessTextureHandle = img.bindlessHandle;

        cmd.vertices = vertices;
        cmd.useCustomVertices = !vertices.empty() &&
            (img.imageType != UIImage::ImageType::SIMPLE ||
                img.fillAmount < 1.0f ||
                std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON);

        if (viewMatrix) cmd.viewMatrix = *viewMatrix;
        if (projMatrix) cmd.projMatrix = *projMatrix;

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && m_transformSystem) {
            cmd.modelMatrix = m_transformSystem->BuildWorldSpaceModelMatrix(entity, canvasEntity, rect, accumulated);
        }

        NE::Graphics::UIRenderer::Submit(cmd);
    }

    void UIRenderSystem::RenderCanvasChildren(
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        std::vector<Entity> canvasChildren = CollectCanvasChildren(canvasEntity);

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && canvasChildren.size() > 1) {
            SortEntitiesByZOrder(canvasChildren);
        }

        for (Entity e : canvasChildren) {
            auto& img = m_cm->GetComponent<UIImage>(e);
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);

            if (!m_transformSystem) continue;

            UITransformSystem::AccumulatedTransform accumulated = m_transformSystem->AccumulateParentTransforms(e, canvasEntity, canvas);

            UITransformSystem::WorldTransform worldTransform = m_transformSystem->CalculateWorldTransform(e, canvasEntity, canvas, viewMatrix, projMatrix);

            std::vector<NE::Graphics::UIVertex> vertices;

            if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                vertices = GenerateWorldSpaceVertices(img);
            }
            else {
                vertices = GenerateScreenSpaceVertices(e, worldTransform, img);
            }

            if (vertices.empty()) continue;

            SubmitDrawCommand(e, canvasEntity, canvas, img, rect, worldTransform, accumulated, vertices, viewMatrix, projMatrix);
        }
    }

    //=========================================================================
    // Vertex Manipulation
    //=========================================================================

    void UIRenderSystem::RotateVertices2D(
        std::vector<NE::Graphics::UIVertex>& vertices,
        float pivotX,
        float pivotY,
        float rotationDegrees
    ) {
        if (std::abs(rotationDegrees) < ROTATION_EPSILON) return;

        float radians = rotationDegrees * PI / 180.0f;
        float cosR = std::cos(radians);
        float sinR = std::sin(radians);

        for (auto& v : vertices) {
            float localX = v.x - pivotX;
            float localY = v.y - pivotY;

            v.x = pivotX + localX * cosR - localY * sinR;
            v.y = pivotY + localX * sinR + localY * cosR;
        }
    }

    //=========================================================================
    // Camera Utilities
    //=========================================================================

    bool UIRenderSystem::GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj) {
        auto* cam = NE::Graphics::GraphicsManager::GetEditorCamera();
        if (!cam) return false;

        outView = cam->GetViewMatrix();
        outProj = cam->GetProjectionMatrix();

        return true;
    }

    //=========================================================================
    // Main Update Loop
    //=========================================================================

    void UIRenderSystem::Update(double) {
        const auto& entities = GetEntities();

        std::vector<std::pair<int, Entity>> canvases;

        for (Entity e : entities) {
            if (!m_cm->HasComponent<UICanvas>(e)) continue;

            auto& canvas = m_cm->GetComponent<UICanvas>(e);
            if (canvas.isActive) {
                canvases.push_back({ canvas.sortingOrder, e });
            }
        }

        std::sort(canvases.begin(), canvases.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [order, canvasEntity] : canvases) {
            auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);

            // Calculate scale factor based on scale mode
            if (m_transformSystem) {
                canvas.scaleFactor = m_transformSystem->CalculateScaleFactor(canvas);

                // Setup canvas defaults based on render mode
                m_transformSystem->SetupCanvasDefaults(canvasEntity, canvas);
            }

            // Get camera matrices if needed
            Math::Mat4 viewMatrix, projMatrix;
            Math::Mat4* pView = nullptr;
            Math::Mat4* pProj = nullptr;

            if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA ||
                canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                if (GetCameraMatrices(viewMatrix, projMatrix)) {
                    pView = &viewMatrix;
                    pProj = &projMatrix;
                }
                else {
                    std::cerr << "[UIRenderSystem] Warning: Canvas requires camera but none found!" << std::endl;
                }
            }

            RenderCanvasChildren(canvasEntity, canvas, pView, pProj);
        }
    }

} // namespace NE::ECS::Systems