#ifndef UI_EVENT_SYSTEM_HPP
#define UI_EVENT_SYSTEM_HPP

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "../Components/UICanvas.hpp"
#include "../Components/UIRectTransform.hpp"
#include "../Components/UIButton.hpp"
#include "../Components/UIImage.hpp"
#include <vector>

namespace NE::ECS::Systems {

    class UIEventSystem final : public System {
    public:
        explicit UIEventSystem(ComponentManager* cm);

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

        Entity GetHoveredEntity() const { return m_hoveredEntity; }
        Entity GetPressedEntity() const { return m_pressedEntity; }

    private:
        ComponentManager* m_cm = nullptr;

        Entity m_hoveredEntity = NO_ENTITY;
        Entity m_pressedEntity = NO_ENTITY;

        struct UIElementInfo {
            Entity entity;
            float worldX;
            float worldY;
            float worldWidth;
            float worldHeight;
            float zOrder;
            Entity canvasEntity;
        };

        std::vector<UIElementInfo> CollectInteractableElements();

        bool PointInRect(float px, float py, const UIElementInfo& element);

        void UpdateButtonStates();

        void ApplyButtonColorToImage(Entity buttonEntity);

        float CalculateScaleFactor(const Component::UICanvas& canvas);

        void CalculateWorldRect(
            Entity entity,
            Entity canvasEntity,
            const Component::UICanvas& canvas,
            float& outX, float& outY,
            float& outWidth, float& outHeight
        );
    };

} // namespace NE::ECS::Systems

#endif // UI_EVENT_SYSTEM_HPP
