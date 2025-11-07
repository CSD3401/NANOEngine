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

    void UIRenderSystem::Update(double) {
        const auto& entities = GetEntities();

        for (Entity e : entities)
        {
            auto& rect = m_cm->GetComponent<UIRectTransform>(e);
            auto& img = m_cm->GetComponent<UIImage>(e);

            NE::Graphics::UIDrawCommand cmd;
            cmd.x = rect.x;
            cmd.y = rect.y;
            cmd.width = rect.width;
            cmd.height = rect.height;
            cmd.color = img.color;
            cmd.material = img.material;
            cmd.order = 0; // later: sorting
            cmd.entityId = e;

            NE::Graphics::UIRenderer::Submit(cmd);
        }

        static bool printed = false;
        if (!printed)
        {
            std::cout << "[UIRenderSystem::Update]" << std::endl;
            std::cout << "  System has " << entities.size() << " registered entities" << std::endl;
            printed = true;
        }
    }

    void UIRenderSystem::Exit() {}
}
