#include "UIRenderSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../../Graphics/Core/UIDrawCommand.hpp" 
#include "../../Graphics/Core/UIRenderer.hpp" 
#include "../../Graphics/Core/GraphicsManager.hpp"
#include "../../Graphics/Core/Camera.hpp"
#include <iostream>

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    UIRenderSystem::UIRenderSystem(ComponentManager* cm) : m_cm(cm) {}

    void UIRenderSystem::OnEntityAdded(Entity) {}
    void UIRenderSystem::OnEntityRemoved(Entity) {}
    void UIRenderSystem::Init() {}

    bool UIRenderSystem::GetCameraMatrices(Math::Mat4& outView, Math::Mat4& outProj) {
        auto* cam = NE::Graphics::GraphicsManager::GetCamera();
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
        result.x = rect.x;
        result.y = rect.y;
        result.width = rect.width;
        result.height = rect.height;
        result.z = 0.0f;

        // accumulate parent positions
        if (rect.parent != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(rect.parent)) 
        {
            auto& parentRect = m_cm->GetComponent<UIRectTransform>(rect.parent);
            result.x += parentRect.x;
            result.y += parentRect.y;
        }

        // apply transformations based on render mode
        switch (canvas.renderMode) {
        case UICanvas::RenderMode::SCREEN_SPACE_OVERLAY:
        {
            // simple 2D screen-space rendering
            // apply scale factor (for different resolutions)
            result.x *= canvas.scaleFactor;
            result.y *= canvas.scaleFactor;
            result.width *= canvas.scaleFactor;
            result.height *= canvas.scaleFactor;

            // pixel perfect snapping
            if (canvas.pixelPerfect) 
            {
                result.x = std::round(result.x);
                result.y = std::round(result.y);
                result.width = std::round(result.width);
                result.height = std::round(result.height);
            }
            break;
        }

        case UICanvas::RenderMode::SCREEN_SPACE_CAMERA:
        {
            // render UI in front of camera at a fixed distance
            if (viewMatrix && projMatrix) 
            {
                // get screen dimensions (temp)
                float screenWidth = 1920.0f;   // TODO: Get from GraphicsManager
                float screenHeight = 1080.0f;

                // convert UI position to NDC (-1 to 1)
                float ndcX = (result.x / screenWidth) * 2.0f - 1.0f;
                float ndcY = (result.y / screenHeight) * 2.0f - 1.0f;

                // place at plane distance from camera
                result.z = canvas.planeDistance;

                // Scale based on distance (perspective scaling)
                float scale = canvas.planeDistance / 100.0f;  // adjust based on your scale
                result.x = ndcX * screenWidth * scale;
                result.y = ndcY * screenHeight * scale;
                result.width *= canvas.scaleFactor * scale;
                result.height *= canvas.scaleFactor * scale;
            }
            else 
            {
                // fallback to overlay mode if no camera
                result.x *= canvas.scaleFactor;
                result.y *= canvas.scaleFactor;
                result.width *= canvas.scaleFactor;
                result.height *= canvas.scaleFactor;
            }

            // pixel perfect snapping
            if (canvas.pixelPerfect)
            {
                result.x = std::round(result.x);
                result.y = std::round(result.y);
                result.width = std::round(result.width);
                result.height = std::round(result.height);
            }

            // plane dist
            // order in layer

            break;
        }

        case UICanvas::RenderMode::WORLD_SPACE:
        {
            // Treat UI as 3D object in world space
            // UI elements have world coordinates and can be occluded by 3D objects

            // apply scale factor
            result.width *= canvas.scaleFactor;
            result.height *= canvas.scaleFactor;

            // Z-depth is preserved (can be set via UIRectTransform.z if you add it)
            // result.z = rect.z;  // If you add Z to UIRectTransform

            // Note: In world space, x/y are world coordinates, not screen pixels
            // The renderer will need to transform these using view-projection matrices

            // order in layer
            break;
        }
        }

        return result;
    }

    float UIRenderSystem::CalculateScaleFactor(const UICanvas& canvas) {
        // get current screen size from GraphicsManager (temp)
        float screenWidth = 1920.0f;   // TODO: Get actual screen size
        float screenHeight = 1080.0f;  // GraphicsManager::GetScreenWidth/Height()

        switch (canvas.scaleMode) {
        case UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE:
        {
            float widthScale = screenWidth / canvas.referenceWidth;
            float heightScale = screenHeight / canvas.referenceHeight;

            // use minimum scale to fit content
            // (You could add a "match" parameter: 0=width, 1=height, 0.5=average) // kiv
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

        // sort by Z-order if needed (for world space or layering)
        // std::sort(canvasChildren.begin(), canvasChildren.end(), ...);

        // render all children
        for (Entity e : canvasChildren) 
        {
            auto& img = m_cm->GetComponent<UIImage>(e);

            //// load texture if not already loaded
            //if (!img.material && !img.texturePath.empty())
            //{
            //    // Create material if doesn't exist
            //    img.material = std::make_shared<NE::Graphics::Material>();

            //    // Load texture from path
            //    auto texture = NE::Graphics::TextureManager::LoadTexture(img.texturePath.string());

            //    if (texture)
            //    {
            //        img.material->SetTexture("albedo", texture);
            //        std::cout << "[UIRenderSystem] Loaded texture: " << img.texturePath << std::endl;
            //    }
            //    else
            //    {
            //        std::cerr << "[UIRenderSystem] Failed to load texture: " << img.texturePath << std::endl;
            //    }
            //}

            // calculate world transform based on render mode
            WorldTransform worldTransform = CalculateWorldTransform(e, canvas, viewMatrix, projMatrix);

            // create draw command
            NE::Graphics::UIDrawCommand cmd;
            cmd.x = worldTransform.x;
            cmd.y = worldTransform.y;
            cmd.z = worldTransform.z; 
            cmd.width = worldTransform.width;
            cmd.height = worldTransform.height;
            cmd.color = img.color;
            cmd.material = img.material;
            cmd.order = canvas.sortingOrder;
            cmd.entityId = e;
            cmd.renderMode = static_cast<int>(canvas.renderMode); // pass render mode to renderer
            cmd.planeDistance = canvas.planeDistance;

            // for camera/world space, include matrices
            if (viewMatrix) cmd.viewMatrix = *viewMatrix;
            if (projMatrix) cmd.projMatrix = *projMatrix;

            // submit draw command to UI renderer
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
