#include "UIRenderSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
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

    // Default anchor at center for Screen Space modes
    static constexpr float DEFAULT_ANCHOR_X = 0.5f;
    static constexpr float DEFAULT_ANCHOR_Y = 0.5f;

    // Default world space canvas scale (so 100x100 UI = 10x10 world units)
    static constexpr float DEFAULT_WORLD_SPACE_SCALE = 0.1f;

    //=========================================================================
    // Lifecycle
    //=========================================================================

    UIRenderSystem::UIRenderSystem(ComponentManager* cm) : m_cm(cm) {}

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

    //=========================================================================
    // Canvas Setup Helpers
    //=========================================================================

    void UIRenderSystem::SetupCanvasDefaults(Entity canvasEntity, UICanvas& canvas) {
        if (!m_cm->HasComponent<UIRectTransform>(canvasEntity)) {
            return;
        }

        // Check if render mode changed or first time setup
        bool renderModeChanged = canvas.hasBeenInitialized &&
            (canvas.renderMode != canvas.lastInitializedMode);

        if (canvas.hasBeenInitialized && !renderModeChanged) {
            // Already initialized and mode hasn't changed - only update screen space position
            if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
                canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA)
            {
                auto& canvasRect = m_cm->GetComponent<UIRectTransform>(canvasEntity);
                float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
                float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();
                canvasRect.x = screenWidth * DEFAULT_ANCHOR_X;
                canvasRect.y = screenHeight * DEFAULT_ANCHOR_Y;
                canvasRect.width = canvas.referenceWidth;
                canvasRect.height = canvas.referenceHeight;
            }
            return;
        }

        // First time OR render mode changed - apply full defaults
        auto& canvasRect = m_cm->GetComponent<UIRectTransform>(canvasEntity);

        switch (canvas.renderMode) {
        case UICanvas::RenderMode::SCREEN_SPACE_OVERLAY:
        case UICanvas::RenderMode::SCREEN_SPACE_CAMERA: {
            float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
            float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

            canvasRect.x = screenWidth * DEFAULT_ANCHOR_X;
            canvasRect.y = screenHeight * DEFAULT_ANCHOR_Y;
            canvasRect.z = 0.f;

            // Reset scale to 1 (scaleFactor handles scaling)
            canvasRect.scaleX = 1.f;
            canvasRect.scaleY = 1.f;
            canvasRect.scaleZ = 1.f;

            canvasRect.width = canvas.referenceWidth;
            canvasRect.height = canvas.referenceHeight;

            // Reset rotation
            canvasRect.rotationX = 0.f;
            canvasRect.rotationY = 0.f;
            canvasRect.rotationZ = 0.f;
            break;
        }

        case UICanvas::RenderMode::WORLD_SPACE: {
            // Apply default world space scale
            canvasRect.scaleX = DEFAULT_WORLD_SPACE_SCALE;
            canvasRect.scaleY = DEFAULT_WORLD_SPACE_SCALE;
            canvasRect.scaleZ = 1.f;

            // Reset position to origin (user can move it)
            canvasRect.x = 0.f;
            canvasRect.y = 0.f;
            canvasRect.z = 0.f;

            // Keep rotation as is or reset
            canvasRect.rotationX = 0.f;
            canvasRect.rotationY = 0.f;
            canvasRect.rotationZ = 0.f;
            break;
        }
        }

        // Mark as initialized with current mode
        canvas.hasBeenInitialized = true;
        canvas.lastInitializedMode = canvas.renderMode;
    }

    //=========================================================================
    // Transform Hierarchy Functions
    //=========================================================================

    bool UIRenderSystem::ShouldIncludeCanvasTransform(UICanvas::RenderMode renderMode) {
        return renderMode == UICanvas::RenderMode::WORLD_SPACE;
    }

    std::vector<Entity> UIRenderSystem::BuildParentChain(
        Entity entity,
        Entity canvasEntity,
        UICanvas::RenderMode renderMode
    ) {
        std::vector<Entity> chain;
        Entity current = entity;

        while (current != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(current)) {
            if (current == canvasEntity && !ShouldIncludeCanvasTransform(renderMode)) {
                break;
            }

            chain.push_back(current);
            current = m_cm->GetComponent<UIRectTransform>(current).parent;
        }

        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    UIRenderSystem::AccumulatedTransform UIRenderSystem::AccumulateParentTransforms(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas
    ) {
        AccumulatedTransform result;

        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return result;
        }

        //=====================================================================
        // WORLD SPACE: Original logic - NO center anchor offset
        // Canvas transform IS included (position, rotation, scale all apply)
        //=====================================================================
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

            for (size_t i = 0; i < chain.size(); ++i) {
                Entity current = chain[i];
                auto& rect = m_cm->GetComponent<UIRectTransform>(current);
                bool isTarget = (current == entity);

                result.scaleX *= rect.scaleX;
                result.scaleY *= rect.scaleY;
                result.scaleZ *= rect.scaleZ;
                result.rotationZ += rect.rotationZ;

                if (isTarget) {
                    float localX = rect.x - rect.width * rect.pivotX;
                    float localY = rect.y - rect.height * rect.pivotY;

                    float parentRotation = result.rotationZ - rect.rotationZ;
                    if (std::abs(parentRotation) > ROTATION_EPSILON) {
                        float rad = parentRotation * PI / 180.0f;
                        float cosR = std::cos(rad);
                        float sinR = std::sin(rad);
                        float rotatedX = localX * cosR - localY * sinR;
                        float rotatedY = localX * sinR + localY * cosR;
                        localX = rotatedX;
                        localY = rotatedY;
                    }

                    float parentScaleX = result.scaleX / rect.scaleX;
                    float parentScaleY = result.scaleY / rect.scaleY;

                    result.posX += localX * parentScaleX;
                    result.posY += localY * parentScaleY;
                    result.posZ += rect.z;
                }
                else {
                    result.posX += rect.x;
                    result.posY += rect.y;
                    result.posZ += rect.z;
                }
            }

            return result;
        }

        //=====================================================================
        // SCREEN SPACE: Apply center anchor and scaleFactor
        // Canvas transform is IGNORED (locked like Unity)
        //=====================================================================
        result.scaleX = canvas.scaleFactor;
        result.scaleY = canvas.scaleFactor;

        std::vector<Entity> chain = BuildParentChain(entity, canvasEntity, canvas.renderMode);

        for (size_t i = 0; i < chain.size(); ++i) {
            Entity current = chain[i];
            auto& rect = m_cm->GetComponent<UIRectTransform>(current);
            bool isTarget = (current == entity);

            result.scaleX *= rect.scaleX;
            result.scaleY *= rect.scaleY;
            result.scaleZ *= rect.scaleZ;
            result.rotationZ += rect.rotationZ;

            // Get parent dimensions for anchor calculation
            float parentWidth = 0.f;
            float parentHeight = 0.f;

            if (i > 0) {
                Entity parentEntity = chain[i - 1];
                auto& parentRect = m_cm->GetComponent<UIRectTransform>(parentEntity);
                parentWidth = parentRect.width;
                parentHeight = parentRect.height;
            }
            else {
                // Direct child of canvas - use screen dimensions in reference coordinates
                parentWidth = NE::Graphics::GraphicsManager::GetScreenWidth() / canvas.scaleFactor;
                parentHeight = NE::Graphics::GraphicsManager::GetScreenHeight() / canvas.scaleFactor;
            }

            // Center anchor offset
            float anchorX = parentWidth * DEFAULT_ANCHOR_X;
            float anchorY = parentHeight * DEFAULT_ANCHOR_Y;

            if (isTarget) {
                float localX = anchorX + rect.x - rect.width * rect.pivotX;
                float localY = anchorY + rect.y - rect.height * rect.pivotY;

                float parentRotation = result.rotationZ - rect.rotationZ;
                if (std::abs(parentRotation) > ROTATION_EPSILON) {
                    float rad = parentRotation * PI / 180.0f;
                    float cosR = std::cos(rad);
                    float sinR = std::sin(rad);
                    float rotatedX = localX * cosR - localY * sinR;
                    float rotatedY = localX * sinR + localY * cosR;
                    localX = rotatedX;
                    localY = rotatedY;
                }

                float parentScaleX = result.scaleX / rect.scaleX;
                float parentScaleY = result.scaleY / rect.scaleY;

                result.posX += localX * parentScaleX;
                result.posY += localY * parentScaleY;
                result.posZ += rect.z;
            }
            else {
                result.posX += anchorX + rect.x;
                result.posY += anchorY + rect.y;
                result.posZ += rect.z;
            }
        }

        return result;
    }

    //=========================================================================
    // World Transform Calculation
    //=========================================================================

    UIRenderSystem::WorldTransform UIRenderSystem::CalculateWorldTransform(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        WorldTransform result;

        if (!m_cm->HasComponent<UIRectTransform>(entity)) {
            return result;
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);
        AccumulatedTransform accumulated = AccumulateParentTransforms(entity, canvasEntity, canvas);

        result.x = accumulated.posX;
        result.y = accumulated.posY;
        result.z = accumulated.posZ;
        result.width = rect.width * accumulated.scaleX;
        result.height = rect.height * accumulated.scaleY;
        result.accumulatedRotationZ = accumulated.rotationZ;
        result.accumulatedScaleX = accumulated.scaleX;
        result.accumulatedScaleY = accumulated.scaleY;

        // Pixel-perfect snapping for screen space only
        if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY ||
            canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA) {
            if (canvas.pixelPerfect) {
                ApplyPixelPerfectSnapping(result);
            }
        }

        return result;
    }

    void UIRenderSystem::ApplyPixelPerfectSnapping(WorldTransform& transform) {
        transform.x = std::round(transform.x);
        transform.y = std::round(transform.y);
        transform.width = std::round(transform.width);
        transform.height = std::round(transform.height);
    }

    //=========================================================================
    // Canvas & Scaling
    //=========================================================================

    float UIRenderSystem::CalculateScaleFactor(const UICanvas& canvas) {
        float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

        switch (canvas.scaleMode) {
        case UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE: {
            float widthScale = screenWidth / canvas.referenceWidth;
            float heightScale = screenHeight / canvas.referenceHeight;
            return std::min(widthScale, heightScale);
        }

        case UICanvas::ScaleMode::CONSTANT_PIXEL_SIZE: {
            return 1.0f;
        }

        case UICanvas::ScaleMode::CONSTANT_PHYSICAL_SIZE: {
            float referenceDPI = 96.0f;
            float currentDPI = 96.0f;
            return currentDPI / referenceDPI;
        }
        }

        return 1.0f;
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
        const WorldTransform& worldTransform,
        const UIImage& img
    ) {
        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        auto vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            worldTransform.x,
            worldTransform.y,
            worldTransform.z,
            worldTransform.width,
            worldTransform.height,
            img.color
        );

        if (!vertices.empty() && std::abs(worldTransform.accumulatedRotationZ) > ROTATION_EPSILON) {
            float pivotX = worldTransform.x + worldTransform.width * rect.pivotX;
            float pivotY = worldTransform.y + worldTransform.height * rect.pivotY;
            RotateVertices2D(vertices, pivotX, pivotY, worldTransform.accumulatedRotationZ);
        }

        return vertices;
    }

    std::vector<NE::Graphics::UIVertex> UIRenderSystem::GenerateWorldSpaceVertices(const UIImage& img) {
        return NE::Graphics::UIImageMeshGenerator::GenerateVertices(
            img,
            0.0f, 0.0f, 0.0f,
            1.0f, 1.0f,
            img.color
        );
    }

    Math::Mat4 UIRenderSystem::BuildWorldSpaceModelMatrix(
        Entity entity,
        Entity canvasEntity,
        const UIRectTransform& rect
    ) {
        Math::Vec3 position = rect.GetPosition();
        Math::Vec3 scale = rect.GetScale();
        Math::Vec2 pivot = rect.GetPivot();

        Math::Vec3 effectiveScale = scale;
        Math::Vec3 accumulatedPosition = position;

        float accumulatedRotationX = rect.rotationX;
        float accumulatedRotationY = rect.rotationY;
        float accumulatedRotationZ = rect.rotationZ;

        Entity parentEntity = rect.parent;
        while (parentEntity != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(parentEntity)) {
            auto& parentRect = m_cm->GetComponent<UIRectTransform>(parentEntity);

            effectiveScale.x *= parentRect.scaleX;
            effectiveScale.y *= parentRect.scaleY;
            effectiveScale.z *= parentRect.scaleZ;

            accumulatedRotationX += parentRect.rotationX;
            accumulatedRotationY += parentRect.rotationY;
            accumulatedRotationZ += parentRect.rotationZ;

            accumulatedPosition.x += parentRect.x;
            accumulatedPosition.y += parentRect.y;
            accumulatedPosition.z += parentRect.z;

            parentEntity = parentRect.parent;
        }

        float pivotOffsetX = -rect.width * pivot.x * effectiveScale.x;
        float pivotOffsetY = -rect.height * pivot.y * effectiveScale.y;

        Math::Mat4 scaleMatrix = Math::Mat4::BuildScaling(
            rect.width * effectiveScale.x,
            rect.height * effectiveScale.y,
            effectiveScale.z
        );

        Math::Mat4 pivotMatrix = Math::Mat4::BuildTranslation(
            pivotOffsetX,
            pivotOffsetY,
            0.0f
        );

        Math::Mat4 rotationX = Math::Mat4::BuildXRotation(accumulatedRotationX * PI / 180.0f);
        Math::Mat4 rotationY = Math::Mat4::BuildYRotation(accumulatedRotationY * PI / 180.0f);
        Math::Mat4 rotationZ = Math::Mat4::BuildZRotation(accumulatedRotationZ * PI / 180.0f);
        Math::Mat4 rotationMatrix = rotationZ * rotationY * rotationX;

        Math::Mat4 translationMatrix = Math::Mat4::BuildTranslation(
            accumulatedPosition.x,
            accumulatedPosition.y,
            accumulatedPosition.z
        );

        return translationMatrix * rotationMatrix * pivotMatrix * scaleMatrix;
    }

    void UIRenderSystem::SubmitDrawCommand(
        Entity entity,
        Entity canvasEntity,
        const UICanvas& canvas,
        const UIImage& img,
        const UIRectTransform& rect,
        const WorldTransform& worldTransform,
        std::vector<NE::Graphics::UIVertex>& vertices,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        NE::Graphics::UIDrawCommand cmd;

        cmd.x = worldTransform.x;
        cmd.y = worldTransform.y;
        cmd.z = worldTransform.z;
        cmd.width = worldTransform.width;
        cmd.height = worldTransform.height;
        cmd.color = img.color;
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

        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
            cmd.modelMatrix = BuildWorldSpaceModelMatrix(entity, canvasEntity, rect);
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

            WorldTransform worldTransform = CalculateWorldTransform(e, canvasEntity, canvas, viewMatrix, projMatrix);

            std::vector<NE::Graphics::UIVertex> vertices;

            if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) {
                vertices = GenerateWorldSpaceVertices(img);
            }
            else {
                vertices = GenerateScreenSpaceVertices(e, worldTransform, img);
            }

            if (vertices.empty()) continue;

            SubmitDrawCommand(e, canvasEntity, canvas, img, rect, worldTransform, vertices, viewMatrix, projMatrix);
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
            canvas.scaleFactor = CalculateScaleFactor(canvas);

            // Setup canvas defaults based on render mode
            SetupCanvasDefaults(canvasEntity, canvas);

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