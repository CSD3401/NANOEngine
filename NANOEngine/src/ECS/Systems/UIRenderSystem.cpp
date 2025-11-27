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

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    UIRenderSystem::UIRenderSystem(ComponentManager* cm) : m_cm(cm) {}

    void UIRenderSystem::OnEntityAdded(Entity e) {
        // only handle uiimage-specific setup (textures, materials)
        if (m_cm->HasComponent<UIImage>(e)) {
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
    }

    void UIRenderSystem::OnEntityRemoved(Entity e) {}

    void UIRenderSystem::Init() {
        // Load resources for all existing UIImage entities
        const auto& entities = GetEntities();
        for (Entity e : entities) {
            if (m_cm->HasComponent<UIImage>(e)) {
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
    }

    void UIRenderSystem::RotateVertices2D(
        std::vector<NE::Graphics::UIVertex>& vertices,
        float pivotX, float pivotY,
        float rotationDegrees)
    {
        if (std::abs(rotationDegrees) < 0.001f) return;

        const float PI = 3.14159265358979f;
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

    bool UIRenderSystem::GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj) {
        auto* cam = NE::Graphics::GraphicsManager::GetEditorCamera();
        if (!cam) return false;

        outView = cam->GetViewMatrix();
        outProj = cam->GetProjectionMatrix();

        return true;
    }

    UIRenderSystem::WorldTransform UIRenderSystem::CalculateWorldTransform(
        Entity entity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        if (!m_cm->HasComponent<UIRectTransform>(entity))
        {
            return { 0.f, 0.f, 0.f, 0.f };
        }

        auto& rect = m_cm->GetComponent<UIRectTransform>(entity);

        WorldTransform result;
        result.x = rect.x - rect.width * rect.pivotX;
        result.y = rect.y - rect.height * rect.pivotY;
        result.width = rect.width;
        result.height = rect.height;
        result.z = rect.z;

        // Apply transformations based on render mode
        if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_OVERLAY || canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA)
        {
            // Accumulate parent positions for overlay
            if (rect.parent != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(rect.parent))
            {
                auto& parentRect = m_cm->GetComponent<UIRectTransform>(rect.parent);
                result.x += parentRect.x;
                result.y += parentRect.y;
            }

            // Pixel perfect snapping
            if (canvas.pixelPerfect)
            {
                result.x = std::round(result.x);
                result.y = std::round(result.y);
                result.width = std::round(result.width);
                result.height = std::round(result.height);
            }
        }
        else if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE)
        {
            // In world space, x/y/z are already world coordinates
            // Accumulate parent transforms if in hierarchy
            if (rect.parent != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(rect.parent))
            {
                auto& parentRect = m_cm->GetComponent<UIRectTransform>(rect.parent);
                result.x += parentRect.x;
                result.y += parentRect.y;
                result.z += parentRect.z;
            }

            // World space uses the rect's width/height directly (in world units)
            // No pixel-to-screen conversion needed
            // Transformation is handled by the model matrix in rendering
        }

        return result;
    }

    float UIRenderSystem::CalculateScaleFactor(const UICanvas& canvas) {
        // get current screen size from GraphicsManager (temp)
        float screenWidth = NE::Graphics::GraphicsManager::GetScreenWidth();
        float screenHeight = NE::Graphics::GraphicsManager::GetScreenHeight();

        switch (canvas.scaleMode) {
        case UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE:
        {
            float widthScale = screenWidth / canvas.referenceWidth;
            float heightScale = screenHeight / canvas.referenceHeight;

            // use minimum scale to fit content
            // (add a "match" parameter: 0=width, 1=height, 0.5=average) // kiv
            return std::min(widthScale, heightScale);
        }

        case UICanvas::ScaleMode::CONSTANT_PIXEL_SIZE:
        {
            return 1.0f;  // no scaling
        }

        case UICanvas::ScaleMode::CONSTANT_PHYSICAL_SIZE:
        {
            // scale based on DPI (physical size on screen)
            float referenceDPI = 96.0f;
            float currentDPI = 96.0f;  // TODO: Get from system // kiv
            return currentDPI / referenceDPI;
        }
        }

        return 1.0f;
    }

    void UIRenderSystem::RenderCanvasChildren(
        Entity canvasEntity,
        const UICanvas& canvas,
        const Math::Mat4* viewMatrix,
        const Math::Mat4* projMatrix
    ) {
        const auto& entities = GetEntities();

        // collect all UI elements that belong to this canvas
        std::vector<Entity> canvasChildren;

        for (Entity e : entities) 
        {
            if (e == canvasEntity) continue;
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
            if (!m_cm->HasComponent<UIImage>(e)) continue;
            if (m_cm->HasComponent<UICanvas>(e)) continue; // skip nested canvases

            // check if entity belongs to this canvas
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);
            uint32_t root = e;
            uint32_t current = rect.parent;

            // if current entity is not the root/parent, back track to find the parent of current entity
            while (current != NO_ENTITY)
            {
                root = current;
                if (!m_cm->HasComponent<UIRectTransform>(current)) break;
                current = m_cm->GetComponent<UIRectTransform>(current).parent;
            }

            // if root is our canvas, this entity belongs to this canvas
            if (root == canvasEntity || rect.parent == canvasEntity) 
            {
                canvasChildren.push_back(e);
            }
        }

        // sort by Z-order for proper layering in world space
        if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE && canvasChildren.size() > 1) {
            std::sort(canvasChildren.begin(), canvasChildren.end(),
                [this](Entity a, Entity b) {
                    auto& rectA = m_cm->GetComponent<UIRectTransform>(a);
                    auto& rectB = m_cm->GetComponent<UIRectTransform>(b);
                    return rectA.z < rectB.z; // render back-to-front (higher Z renders later, on top)
                });
        }

        // render all children
        for (Entity e : canvasChildren) 
        {
            auto& img = m_cm->GetComponent<UIImage>(e);
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);
            WorldTransform worldTransform = CalculateWorldTransform(e, canvas, viewMatrix, projMatrix);

            // generate mesh based on UIImage's image type
            std::vector<NE::Graphics::UIVertex> vertices;

            if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) 
            {
                // for world space, generate unit quad vertices (0-1 range)
                // The model matrix will scale/position them correctly
                vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
                    img,
                    0.0f,  // Unit quad origin
                    0.0f,
                    0.0f,
                    1.0f,  // Unit size
                    1.0f,
                    img.color
                );
            }
            else 
            {
                // for screen space, use pixel coordinates
                vertices = NE::Graphics::UIImageMeshGenerator::GenerateVertices(
                    img,
                    worldTransform.x,
                    worldTransform.y,
                    worldTransform.z,
                    worldTransform.width,
                    worldTransform.height,
                    img.color
                );

                // ADD THIS: Apply rotation for overlay mode
                if (!vertices.empty() && std::abs(rect.rotationZ) > 0.001f)
                {
                    float pivotX = worldTransform.x + worldTransform.width * rect.pivotX;
                    float pivotY = worldTransform.y + worldTransform.height * rect.pivotY;
                    RotateVertices2D(vertices, pivotX, pivotY, rect.rotationZ);
                }
            }

            // if no vertices (e.g. fillAmount = 0), skip rendering
            if (vertices.empty()) continue;

            // create draw command
            NE::Graphics::UIDrawCommand cmd;
            cmd.x = worldTransform.x;
            cmd.y = worldTransform.y;
            cmd.z = worldTransform.z;
            cmd.width = worldTransform.width;
            cmd.height = worldTransform.height;
            cmd.color = img.color;
            cmd.order = canvas.sortingOrder;
            cmd.entityId = e;
            cmd.renderMode = static_cast<int>(canvas.renderMode);
            cmd.planeDistance = canvas.planeDistance;

            // pass the material & bindless text handle that contains the texture
            cmd.material = img.material;
            cmd.bindlessTextureHandle = img.bindlessHandle;

            // pass vertex data
            cmd.vertices = vertices;
            cmd.useCustomVertices = !vertices.empty() && (img.imageType != UIImage::ImageType::SIMPLE || img.fillAmount < 1.0f || std::abs(rect.rotationZ) > 0.001f);

            // include matrices for all modes
            if (viewMatrix) cmd.viewMatrix = *viewMatrix;
            if (projMatrix) cmd.projMatrix = *projMatrix;

            // build model matrix for world space
            if (canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) 
            {
                // Get transform components
                Math::Vec3 position = rect.GetPosition();
                Math::Vec3 scale = rect.GetScale();
                Math::Vec2 pivot = rect.GetPivot();

                // === accumulate scale from parent hierarchy ===
                // Calculate effective scale by multiplying parent scales
                Math::Vec3 effectiveScale = scale;

                uint32_t parentEntity = rect.parent;
                while (parentEntity != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(parentEntity)) {
                    auto& parentRect = m_cm->GetComponent<UIRectTransform>(parentEntity);
                    effectiveScale.x *= parentRect.scaleX;
                    effectiveScale.y *= parentRect.scaleY;
                    effectiveScale.z *= parentRect.scaleZ;
                    parentEntity = parentRect.parent;
                }


                // Calculate pivot offset in world units
                // Pivot determines where the element rotates/scales around
                float pivotOffsetX = -rect.width * pivot.x * effectiveScale.x;
                float pivotOffsetY = -rect.height * pivot.y * effectiveScale.y;

                // Build transformation matrix: T * R * S * PivotOffset
                // 1. Scale (apply both rect size and EFFECTIVE scale factors)
                Math::Mat4 scaleMatrix = Math::Mat4::BuildScaling(
                    rect.width * effectiveScale.x,
                    rect.height * effectiveScale.y,
                    effectiveScale.z
                );

                // 2. Apply pivot offset (translate to pivot point before rotation)
                Math::Mat4 pivotMatrix = Math::Mat4::BuildTranslation(
                    pivotOffsetX,
                    pivotOffsetY,
                    0.0f
                );

                // 3. Rotation (from Euler angles)
                Math::Mat4 rotationMatrix = rect.GetRotationMatrix();

                // 4. Translation to world position
                Math::Mat4 translationMatrix = Math::Mat4::BuildTranslation(
                    position.x,
                    position.y,
                    position.z
                );

                // Combine: Translation * Rotation * Pivot * Scale
                // This order ensures the UI rotates around its pivot point
                cmd.modelMatrix = translationMatrix * rotationMatrix * pivotMatrix * scaleMatrix;
            }

            NE::Graphics::UIRenderer::Submit(cmd);
        }
    }

    void UIRenderSystem::Update(double) {
        const auto& entities = GetEntities();

        // find and sort all active canvases
        std::vector<std::pair<int, Entity>> canvases;
        for (Entity e : entities) 
        {
            if (m_cm->HasComponent<UICanvas>(e))
            {
                auto& canvas = m_cm->GetComponent<UICanvas>(e);
                if (canvas.isActive)
                {
                    canvases.push_back({ canvas.sortingOrder, e });
                }
            }
        }

        // sort by order (lower renders first, appears behind)
        std::sort(canvases.begin(), canvases.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        // render each canvas
        for (const auto& [order, canvasEntity] : canvases) 
        {
            auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);

            // calculate scale factor based on scale mode
            canvas.scaleFactor = CalculateScaleFactor(canvas);

            Math::Mat4 viewMatrix, projMatrix;
            Math::Mat4* pView = nullptr;
            Math::Mat4* pProj = nullptr;

            // get camera matrices if needed
            if (canvas.renderMode == UICanvas::RenderMode::SCREEN_SPACE_CAMERA || canvas.renderMode == UICanvas::RenderMode::WORLD_SPACE) 
            {
                if (GetCameraMatrices(viewMatrix, projMatrix))
                {
                    pView = &viewMatrix;
                    pProj = &projMatrix;

                    // DEBUG
                    static bool cameraPrinted = false;
                    if (!cameraPrinted) {
                        std::cout << "[World Space Camera Debug]" << std::endl;
                        std::cout << "  View Matrix valid: " << (pView != nullptr) << std::endl;
                        std::cout << "  Proj Matrix valid: " << (pProj != nullptr) << std::endl;
                        cameraPrinted = true;
                    }
                }
                else 
                {
                    std::cerr << "[UIRenderSystem] Warning: Canvas requires camera but none found!" << std::endl;
                }
            }

            // render all children with appropriate matrices
            RenderCanvasChildren(canvasEntity, canvas, pView, pProj);
        }

        static bool printed = false;
        if (!printed) {
            std::cout << "[UIRenderSystem::Update] Found " << canvases.size() << " active canvases" << std::endl;
            printed = true;
        }
    }

    void UIRenderSystem::Exit() {}
}


//## Visual Example of Complete Flow
//Frame Update :
//1. Find Canvases -> [Canvas_A(order = 0), Canvas_B(order = 5)]
//2. Sort -> [Canvas_A, Canvas_B]
//
//For Canvas_A :
//3. RenderCanvasChildren(Canvas_A)
//- Find children -> [Image1, Image2]
//
//For Image1 :
//4. CalculateWorldTransform(Image1)
//- Local : (10, 20) size 50x50
//- Parent offsets : +100, +50
//- World : (110, 70) size 50x50
//- Scale : 0.8x
//- Final : (88, 56) size 40x40
//
//5. Submit draw command -> UIRenderer
//
//(Repeat for Image2)
//
//For Canvas_B :
//6. (Same process, renders on top since order = 5)
