#pragma once
#include <iostream>
#include "EngineAPI.hpp"
#include "../../NANOEngine/src/ECS/Components/Transform.hpp"
#include "../../NANOEngine/src/ECS/Components/Light.hpp"
#include "../../NANOEngine/src/EditorInterface/ECSExports.hpp"


class LightSwitch : public NE::Scripting::IScript {
public:
    LightSwitch() {
    }

    void Initialize(NE::Scripting::Entity entity) override {

    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        if (NE::InputManager::WasKeyPressed('E')) {
            auto& light = NE::ECS::Command::GetEntityLight(GetEntity());
            if (!switched) {
                light.color = NE::Math::Vec3(0.7f, 0.4f, 0.f);
            } else {
                light.color = NE::Math::Vec3(1.f, 1.f, 1.f);
            }

            switched = !switched;
        }

    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "LightSwitch";
    }

    // Event handlers (required by interface)
    void OnCollisionEnter(NE::Scripting::Entity other) override {}
    void OnCollisionExit(NE::Scripting::Entity other) override {}
    void OnTriggerEnter(NE::Scripting::Entity other) override {}
    void OnTriggerExit(NE::Scripting::Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    //std::string objectName = "TestObject";

    bool switched = false;
};
