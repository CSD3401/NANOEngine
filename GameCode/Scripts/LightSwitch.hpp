#pragma once
#include <iostream>
#include "Scripting/IScript.hpp"
#include "ECS/Components/Transform.hpp"
#include "Events/EventBus.hpp"
#include "Core/Couroutine.hpp"
#include <Math/Vec3.hpp>
#include <Input/InputManager.hpp>
#include <ECS/Components/Light.hpp>
#include <EditorInterface/ECSExports.hpp>


class LightSwitch : public IScript {
public:
    LightSwitch() {
    }

    void Initialize(NE::ECS::Entity entity) override {

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
    void OnCollisionEnter(NE::ECS::Entity other) override {}
    void OnCollisionExit(NE::ECS::Entity other) override {}
    void OnTriggerEnter(NE::ECS::Entity other) override {}
    void OnTriggerExit(NE::ECS::Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    //std::string objectName = "TestObject";

    bool switched = false;
};
