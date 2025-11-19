#pragma once
#include <iostream>
#include "EngineAPI.hpp"

class Gears : public IScript {
public:
    Gears() {
    }

    void Initialize(Entity entity) override {

    }

    void Update(double deltaTime) override {
        if (!isActive) return;

        if (Input::WasKeyPressed('E')) {
            auto& transform = Command::GetEntityTransform(GetEntity());

            // Explicitly convert Math::Vec3 to Scripting::Vec3 for comparison
            if (transform.scale == NE::Math::Vec3(0.f, 0.f, 0.f)) {
                transform.scale = Vec3(0.2f, 0.2f, 0.2f);
            } else {
                transform.scale = Vec3(0.f, 0.f, 0.f);
            }
            //if (!switched) {
            //    light.color = Vec3(0.7f, 0.4f, 0.f);
            //} else {
            //    light.color = Vec3(1.f, 1.f, 1.f);
            //}

            //switched = !switched;
        }

    }

    void OnDestroy() override {

    }

    const char* GetTypeName() const override {
        return "Gears";
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
