#pragma once

#include "../Core/System.hpp"
#include "../Core/ComponentManager.hpp"
#include "UILayoutEngine.hpp"

namespace NE::ECS::Systems {

    class UILayoutSystem final : public System {
    public:
        explicit UILayoutSystem(ComponentManager* cm);

        void SetLayoutEngine(UILayoutEngine* engine) { m_layoutEngine = engine; }

        void Init() override;
        void Update(double deltaTime) override;
        void Exit() override;
        void OnEntityAdded(Entity e) override;
        void OnEntityRemoved(Entity e) override;

    private:
        void ProcessAutoSize();
        void ProcessLinearLayout(Entity entity, bool isHorizontal);
        void ProcessGridLayout(Entity entity);

        ComponentManager* m_cm;
        UILayoutEngine* m_layoutEngine = nullptr;
    };

} // namespace NE::ECS::Systems
