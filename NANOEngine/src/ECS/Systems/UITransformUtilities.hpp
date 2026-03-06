#pragma once
#include "ECS/Core/ComponentManager.hpp"
#include "ECS/Core/EntityManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
//#include "../Components/EntityMeta.hpp"
#include "../Components/Hierarchy.hpp"
#include "../../Graphics/Core/GraphicsManager.hpp"
#include <algorithm>

namespace NE::ECS::UIUtil {

    // Check if an entity (and all parents up to the canvas) are active
    inline bool IsActiveForUI(
        ComponentManager* cm,
        EntityManager* em,
        Entity entity,
        Entity canvasEntity
    )
    {
        Entity cur = entity;

        while (cur != NO_ENTITY)
        {
            //if (cm->HasComponent<Component::EntityMeta>(cur)) {
            //    if (!cm->GetComponent<Component::EntityMeta>(cur).isActive) {
            //        return false;
            //    }
            //}

            if (!em->GetActive(cur)) return false;

            if (cur == canvasEntity) break;

            if (!cm->HasComponent<Component::Hierarchy>(cur)) break;
            cur = cm->GetComponent<Component::Hierarchy>(cur).parent;
        }

        // Also require canvas itself to be active
        if (canvasEntity != NO_ENTITY && cm->HasComponent<Component::UICanvas>(canvasEntity)) {
            const auto& canvas = cm->GetComponent<Component::UICanvas>(canvasEntity);

            bool metaActive = em->GetActive(canvasEntity);
            //metaActive = 
            //if (cm->HasComponent<Component::EntityMeta>(canvasEntity)) {
            //}

            if (!canvas.isActive || !metaActive) return false;
        }

        return true;
    }

    // Compute canvas scale factor from screen size and canvas settings
    inline float CalculateScaleFactor(const Component::UICanvas& canvas)
    {
        float screenWidth  = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenWidth());
        float screenHeight = static_cast<float>(NE::Graphics::GraphicsManager::GetScreenHeight());

        switch (canvas.scaleMode) {
        case Component::UICanvas::ScaleMode::SCALE_WITH_SCREEN_SIZE: {
            float widthScale  = screenWidth  / canvas.referenceWidth;
            float heightScale = screenHeight / canvas.referenceHeight;
            return std::min(widthScale, heightScale);
        }
        case Component::UICanvas::ScaleMode::CONSTANT_PIXEL_SIZE:
            return 1.0f;
        case Component::UICanvas::ScaleMode::CONSTANT_PHYSICAL_SIZE: {
            float referenceDPI = 96.0f;
            float currentDPI   = 96.0f; // TODO: query actual DPI
            return currentDPI / referenceDPI;
        }
        }

        return 1.0f;
    }

} // namespace NE::ECS::UIUtil
