#include "UIRenderSystem.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIImage.hpp"
#include "../../Graphics/Core/UIDrawCommand.hpp" 
#include "../../Graphics/Core/UIRenderer.hpp" 
#include "../../Graphics/Core/GraphicsManager.hpp"
#include <iostream>

using namespace NE::ECS;
using namespace NE::ECS::Component;

namespace NE::ECS::Systems {

    UIRenderSystem::UIRenderSystem(ComponentManager* cm) : m_cm(cm) {}

    void UIRenderSystem::OnEntityAdded(Entity) {}
    void UIRenderSystem::OnEntityRemoved(Entity) {}
    void UIRenderSystem::Init() {}

    UIRenderSystem::WorldTransform UIRenderSystem::CalculateWorldTransform(Entity entity, const UICanvas& canvas) {
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

        // position add up with parent's position offset
        if (rect.parent != NO_ENTITY && m_cm->HasComponent<UIRectTransform>(rect.parent)) 
        {
            auto& parentRect = m_cm->GetComponent<UIRectTransform>(rect.parent);
            result.x += parentRect.x;
            result.y += parentRect.y;
        }

        // apply canvas scale factor
        result.x *= canvas.scaleFactor;
        result.y *= canvas.scaleFactor;
        result.width *= canvas.scaleFactor;
        result.height *= canvas.scaleFactor;

        return result;
    }


    float UIRenderSystem::CalculateScaleFactor(const UICanvas& canvas) {
        // Get current screen size (you'll need to pass this in or get from GraphicsManager)
        float screenWidth = 1920.0f;  // TODO: Get actual screen size
        float screenHeight = 1080.0f;

        if (canvas.scaleMode == UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE) 
        {
            float widthScale = screenWidth / canvas.referenceWidth;
            float heightScale = screenHeight / canvas.referenceHeight;
            return std::min(widthScale, heightScale);
        }

        return 1.0f;
    }

    void UIRenderSystem::RenderCanvasChildren(Entity canvasEntity, const UICanvas& canvas) {
        const auto& entities = GetEntities();

        // collect all UI elements that belong to this canvas hierarchy
        std::vector<Entity> canvasChildren;

        for (Entity e : entities) 
        {
            // skip the canvas itself
            if (e == canvasEntity) continue;

            // must have both rect transform and image
            if (!m_cm->HasComponent<UIRectTransform>(e)) continue;
            if (!m_cm->HasComponent<UIImage>(e)) continue;

            // skip other canvases
            if (m_cm->HasComponent<UICanvas>(e)) continue;

            // check if this entity belongs to this canvas
            // Walk up parent chain to find root
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);
            uint32_t root = e;
            uint32_t current = rect.parent;

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

        // render all children
        for (Entity e : canvasChildren) 
        {
            auto& img = m_cm->GetComponent<UIImage>(e);

            // calculate world position (parent-relative (local) --> absolute (screen))
            WorldTransform worldTransform = CalculateWorldTransform(e, canvas);

            // create draw command
            NE::Graphics::UIDrawCommand cmd;
            cmd.x = worldTransform.x;
            cmd.y = worldTransform.y;
            cmd.width = worldTransform.width;
            cmd.height = worldTransform.height;
            cmd.color = img.color;
            cmd.material = img.material;
            cmd.order = canvas.sortingOrder;
            cmd.entityId = e;

            // send to UI renderer
            NE::Graphics::UIRenderer::Submit(cmd);
        }
    }

    void UIRenderSystem::Update(double) {
        const auto& entities = GetEntities();

        // first pass: find all canvases and sort by order
        std::vector<std::pair<int, Entity>> canvases;
        for (Entity e : entities) 
        {
            if (m_cm->HasComponent<UICanvas>(e)) 
            {
                auto& canvas = m_cm->GetComponent<UICanvas>(e);
                if (canvas.isActive) // only collect the active ones
                { 
                    canvases.push_back({ canvas.sortingOrder, e });
                }
            }
        }

        // sort canvases by order (lower order renders first)
        std::sort(canvases.begin(), canvases.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

        // second pass: render UI elements belonging to each canvas
        for (const auto& [order, canvasEntity] : canvases) 
        {
            auto& canvas = m_cm->GetComponent<UICanvas>(canvasEntity);

            // calculate scale factor based on screen size
            canvas.scaleFactor = CalculateScaleFactor(canvas);

            // render all children of this canvas
            RenderCanvasChildren(canvasEntity, canvas);
        }

        static bool printed = false;
        if (!printed) 
        {
            std::cout << "[UIRenderSystem::Update]" << std::endl;
            std::cout << "  Found " << canvases.size() << " active canvases" << std::endl;
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
