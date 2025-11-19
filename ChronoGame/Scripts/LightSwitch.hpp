#pragma once
#include <iostream>
#include "EngineAPI.hpp"

class LightSwitch : public IScript {
public:
    LightSwitch() {
    }

    void Initialize(Entity entity) override {

    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        if (Input::WasKeyPressed('E')) {
            auto& light = Command::GetEntityLight(GetEntity());
            if (!switched) {
                light.color = Vec3(0.7f, 0.4f, 0.f);
            } else {
                light.color = Vec3(1.f, 1.f, 1.f);
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
    void OnCollisionEnter(Entity other) override {}
    void OnCollisionExit(Entity other) override {}
    void OnTriggerEnter(Entity other) override {}
    void OnTriggerExit(Entity other) override {}

private:
    // === Exposed Fields ===
    // These will automatically appear in the editor inspector
    bool isActive = true;
    //std::string objectName = "TestObject";

    bool switched = false;
};
